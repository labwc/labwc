// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_viewporter.h>
#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#include "xwayland-shell-v1-protocol.h"
#endif
#include "drm-lease-v1-protocol.h"
#include "config/rcxml.h"
#include "config/session.h"
#include "decorations.h"
#include "idle.h"
#include "labwc.h"
#include "layers.h"
#include "menu/menu.h"
#include "output-virtual.h"
#include "regions.h"
#include "resize_indicator.h"
#include "theme.h"
#include "view.h"
#include "workspaces.h"
#include "xwayland.h"

#define LAB_WLR_COMPOSITOR_VERSION 5
#define LAB_WLR_FRACTIONAL_SCALE_V1_VERSION 1

static struct wlr_compositor *compositor;
static struct wl_event_source *sighup_source;
static struct wl_event_source *sigint_source;
static struct wl_event_source *sigterm_source;
static struct wl_event_source *sigchld_source;

static void
reload_config_and_theme(struct server *server)
{
	rcxml_finish();
	rcxml_read(rc.config_file);
	theme_finish(server->theme);
	theme_init(server->theme, rc.theme_name);

	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		view_reload_ssd(view);
	}

	menu_reconfigure(server);
	seat_reconfigure(server);
	regions_reconfigure(server);
	resize_indicator_reconfigure(server);
	kde_server_decoration_update_default();
	workspaces_reconfigure(server);
}

static int
handle_sighup(int signal, void *data)
{
	struct server *server = data;

	session_environment_init();
	reload_config_and_theme(server);
	output_virtual_update_fallback(server);
	return 0;
}

static int
handle_sigterm(int signal, void *data)
{
	struct wl_display *display = data;

	wl_display_terminate(display);
	return 0;
}

static int
handle_sigchld(int signal, void *data)
{
	siginfo_t info;
	info.si_pid = 0;
	struct server *server = data;

	/* First call waitid() with NOWAIT which doesn't consume the zombie */
	if (waitid(P_ALL, /*id*/ 0, &info, WEXITED | WNOHANG | WNOWAIT) == -1) {
		return 0;
	}

	if (info.si_pid == 0) {
		/* No children in waitable state */
		return 0;
	}

#if HAVE_XWAYLAND
	/* Ensure that we do not break xwayland lazy initialization */
	if (server->xwayland && server->xwayland->server
			&& info.si_pid == server->xwayland->server->pid) {
		return 0;
	}
#endif

	/* And then do the actual (consuming) lookup again */
	int ret = waitid(P_PID, info.si_pid, &info, WEXITED);
	if (ret == -1) {
		wlr_log(WLR_ERROR, "blocking waitid() for %ld failed: %d",
			(long)info.si_pid, ret);
		return 0;
	}

	switch (info.si_code) {
	case CLD_EXITED:
		wlr_log(info.si_status == 0 ? WLR_DEBUG : WLR_ERROR,
			"spawned child %ld exited with %d",
			(long)info.si_pid, info.si_status);
		break;
	case CLD_KILLED:
	case CLD_DUMPED:
		; /* works around "a label can only be part of a statement" */
		const char *signame = strsignal(info.si_status);
		wlr_log(WLR_ERROR,
			"spawned child %ld terminated with signal %d (%s)",
				(long)info.si_pid, info.si_status,
				signame ? signame : "unknown");
		break;
	default:
		wlr_log(WLR_ERROR,
			"spawned child %ld terminated unexpectedly: %d"
			" please report", (long)info.si_pid, info.si_code);
	}

	if (info.si_pid == server->primary_client_pid) {
		wlr_log(WLR_INFO, "primary client %ld exited", (long)info.si_pid);
		wl_display_terminate(server->wl_display);
	}

	return 0;
}

static void
seat_inhibit_input(struct seat *seat,  struct wl_client *active_client)
{
	seat->active_client_while_inhibited = active_client;

	if (seat->focused_layer && active_client !=
			wl_resource_get_client(seat->focused_layer->resource)) {
		seat_set_focus_layer(seat, NULL);
	}
	struct wlr_surface *previous_kb_surface =
		seat->seat->keyboard_state.focused_surface;
	if (previous_kb_surface && active_client !=
			wl_resource_get_client(previous_kb_surface->resource)) {
		seat_focus_surface(seat, NULL);	  /* keyboard focus */
	}

	struct wlr_seat_client *previous_ptr_client =
		seat->seat->pointer_state.focused_client;
	if (previous_ptr_client && previous_ptr_client->client != active_client) {
		wlr_seat_pointer_clear_focus(seat->seat);
	}
}

static void
seat_disinhibit_input(struct seat *seat)
{
	seat->active_client_while_inhibited = NULL;

	/*
	 * Triggers a refocus of the topmost surface layer if necessary
	 * TODO: Make layer surface focus per-output based on cursor position
	 */
	output_update_all_usable_areas(seat->server, /*layout_changed*/ false);
}

static void
handle_input_inhibit(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_INFO, "activate input inhibit");

	struct server *server =
		wl_container_of(listener, server, input_inhibit_activate);
	seat_inhibit_input(&server->seat, server->input_inhibit->active_client);
}

static void
handle_input_disinhibit(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_INFO, "deactivate input inhibit");

	struct server *server =
		wl_container_of(listener, server, input_inhibit_deactivate);
	seat_disinhibit_input(&server->seat);
}

static void
handle_drm_lease_request(struct wl_listener *listener, void *data)
{
	struct wlr_drm_lease_request_v1 *req = data;
	struct wlr_drm_lease_v1 *lease = wlr_drm_lease_request_v1_grant(req);
	if (!lease) {
		wlr_log(WLR_ERROR, "Failed to grant lease request");
		wlr_drm_lease_request_v1_reject(req);
		return;
	}

	for (size_t i = 0; i < req->n_connectors; ++i) {
		struct output *output = req->connectors[i]->output->data;
		if (!output) {
			continue;
		}

		wlr_output_enable(output->wlr_output, false);
		wlr_output_commit(output->wlr_output);

		wlr_output_layout_remove(output->server->output_layout,
			output->wlr_output);
		output->scene_output = NULL;

		output->leased = true;
	}
}

static bool
server_global_filter(const struct wl_client *client, const struct wl_global *global, void *data)
{
	const struct wl_interface *iface = wl_global_get_interface(global);
	struct server *server = (struct server *)data;
	/* Silence unused var compiler warnings */
	(void)iface; (void)server;

#if HAVE_XWAYLAND
	struct wl_client *xwayland_client = (server->xwayland && server->xwayland->server)
		? server->xwayland->server->client
		: NULL;

	if (client == xwayland_client) {
		/*
		 * Filter out wp_drm_lease_device_v1 for now as it is resulting in
		 * issues with Xwayland applications lagging over time.
		 *
		 * https://github.com/labwc/labwc/issues/553
		 */
		if (!strcmp(iface->name, wp_drm_lease_device_v1_interface.name)) {
			return false;
		}
	} else if (!strcmp(iface->name, xwayland_shell_v1_interface.name)) {
		/* Filter out the xwayland shell for usual clients */
		return false;
	}
#endif

	return true;
}

/*
 * This message is intended to help users who are trying labwc on
 * clean/minimalist systems without existing Desktop Environments (possibly
 * through Virtual Managers) where polkit is missing or GPU drivers do not
 * exist, in the hope that it will reduce the time required to get labwc running
 * and prevent some troubleshooting steps.
 */
static const char helpful_seat_error_message[] =
"\n"
"Some friendly trouble-shooting help\n"
"===================================\n"
"\n"
"If a seat could not be created, this may be caused by lack of permission to the\n"
"seat, input and video groups. If you are using a systemd setup, try installing\n"
"polkit (sometimes called policykit-1). For other setups, search your OS/Distro's\n"
"documentation on how to use seatd, elogind or similar. This is likely to involve\n"
"manually adding users to groups.\n"
"\n"
"If the above does not work, try running with `WLR_RENDERER=pixman labwc` in\n"
"order to use the software rendering fallback\n";

static void
get_headless_backend(struct wlr_backend *backend, void *data)
{
	if (wlr_backend_is_headless(backend)) {
		struct wlr_backend **headless = data;
		*headless = backend;
	}
}

void
server_init(struct server *server)
{
	server->primary_client_pid = -1;
	server->wl_display = wl_display_create();
	if (!server->wl_display) {
		wlr_log(WLR_ERROR, "cannot allocate a wayland display");
		exit(EXIT_FAILURE);
	}

	wl_display_set_global_filter(server->wl_display, server_global_filter, server);

	/* Catch SIGHUP */
	struct wl_event_loop *event_loop = NULL;
	event_loop = wl_display_get_event_loop(server->wl_display);
	sighup_source = wl_event_loop_add_signal(
		event_loop, SIGHUP, handle_sighup, server);
	sigint_source = wl_event_loop_add_signal(
		event_loop, SIGINT, handle_sigterm, server->wl_display);
	sigterm_source = wl_event_loop_add_signal(
		event_loop, SIGTERM, handle_sigterm, server->wl_display);
	sigchld_source = wl_event_loop_add_signal(
		event_loop, SIGCHLD, handle_sigchld, server);
	server->wl_event_loop = event_loop;

	/*
	 * Prevent wayland clients that request the X11 clipboard but closing
	 * their read fd prematurely to crash labwc because of the unhandled
	 * SIGPIPE signal. It is caused by wlroots trying to write the X11
	 * clipboard data to the closed fd of the wayland client.
	 * See https://github.com/labwc/labwc/issues/890#issuecomment-1524962995
	 * for a reproducer involving xclip and wl-paste | head -c 1.
	 */
	signal(SIGPIPE, SIG_IGN);

	/*
	 * The backend is a feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an x11
	 * window if an x11 server is running.
	 */
	server->backend = wlr_backend_autocreate(
		server->wl_display, &server->session);
	if (!server->backend) {
		wlr_log(WLR_ERROR, "unable to create backend");
		fprintf(stderr, helpful_seat_error_message);
		exit(EXIT_FAILURE);
	}

	/* Create headless backend to enable adding virtual outputs later on */
	wlr_multi_for_each_backend(server->backend,
		get_headless_backend, &server->headless.backend);

	if (!server->headless.backend) {
		wlr_log(WLR_DEBUG, "manually creating headless backend");
		server->headless.backend = wlr_headless_backend_create(server->wl_display);
	} else {
		wlr_log(WLR_DEBUG, "headless backend already exists");
	}

	if (!server->headless.backend) {
		wlr_log(WLR_ERROR, "unable to create headless backend");
		exit(EXIT_FAILURE);
	}
	wlr_multi_backend_add(server->backend, server->headless.backend);

	/*
	 * If we don't populate headless backend with a virtual output (that we
	 * create and immediately destroy), then virtual outputs being added
	 * later do not work properly when overlaid on real output. Content is
	 * drawn on the virtual output, but not drawn on the real output.
	 */
	wlr_output_destroy(wlr_headless_add_output(server->headless.backend, 0, 0));

	/*
	 * Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The
	 * user can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients.
	 */
	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		wlr_log(WLR_ERROR, "unable to create renderer");
		exit(EXIT_FAILURE);
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	/*
	 * Autocreates an allocator for us. The allocator is the bridge between
	 * the renderer and the backend. It handles the buffer creation,
	 * allowing wlroots to render onto the screen
	 */
	server->allocator = wlr_allocator_autocreate(
		server->backend, server->renderer);
	if (!server->allocator) {
		wlr_log(WLR_ERROR, "unable to create allocator");
		exit(EXIT_FAILURE);
	}

	wl_list_init(&server->views);
	wl_list_init(&server->unmanaged_surfaces);

	server->ssd_hover_state = ssd_hover_state_new();

	server->scene = wlr_scene_create();
	if (!server->scene) {
		wlr_log(WLR_ERROR, "unable to create scene");
		exit(EXIT_FAILURE);
	}

	/*
	 * The order in which the scene-trees below are created determines the
	 * z-order for nodes which cover the whole work-area.  For per-output
	 * scene-trees, see new_output_notify() in src/output.c
	 *
	 * | Type              | Scene Tree       | Per Output | Example
	 * | ----------------- | ---------------- | ---------- | -------
	 * | ext-session       | lock-screen      | Yes        | swaylock
	 * | layer-shell       | layer-popups     | Yes        |
	 * | layer-shell       | overlay-layer    | Yes        |
	 * | layer-shell       | top-layer        | Yes        | waybar
	 * | server            | labwc-menus      | No         |
	 * | xwayland-OR       | unmanaged        | No         | dmenu
	 * | xdg-popups        | xdg-popups       | No         |
	 * | toplevels windows | always-on-top    | No         |
	 * | toplevels windows | normal           | No         | firefox
	 * | toplevels windows | always-on-bottom | No         | pcmanfm-qt --desktop
	 * | layer-shell       | bottom-layer     | Yes        | waybar
	 * | layer-shell       | background-layer | Yes        | swaybg
	 */

	server->view_tree_always_on_bottom = wlr_scene_tree_create(&server->scene->tree);
	server->view_tree = wlr_scene_tree_create(&server->scene->tree);
	server->view_tree_always_on_top = wlr_scene_tree_create(&server->scene->tree);
	server->xdg_popup_tree = wlr_scene_tree_create(&server->scene->tree);
#if HAVE_XWAYLAND
	server->unmanaged_tree = wlr_scene_tree_create(&server->scene->tree);
#endif
	server->menu_tree = wlr_scene_tree_create(&server->scene->tree);

	workspaces_init(server);

	output_init(server);

	/*
	 * Create some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device
	 * manager handles the clipboard. Each of these wlroots interfaces has
	 * room for you to dig your fingers in and play with their behavior if
	 * you want.
	 */
	compositor = wlr_compositor_create(server->wl_display,
		LAB_WLR_COMPOSITOR_VERSION, server->renderer);
	if (!compositor) {
		wlr_log(WLR_ERROR, "unable to create the wlroots compositor");
		exit(EXIT_FAILURE);
	}
	wlr_subcompositor_create(server->wl_display);

	struct wlr_data_device_manager *device_manager = NULL;
	device_manager = wlr_data_device_manager_create(server->wl_display);
	if (!device_manager) {
		wlr_log(WLR_ERROR, "unable to create data device manager");
		exit(EXIT_FAILURE);
	}

	/*
	 * Empirically, primary selection doesn't work with Gtk apps unless the
	 * device manager is one of the earliest globals to be advertised. All
	 * credit to Wayfire for discovering this, though their symptoms
	 * (crash) are not the same as ours (silently does nothing). When adding
	 * more globals above this line it would be as well to check that
	 * middle-button paste still works with any Gtk app of your choice
	 *
	 * https://wayfire.org/2020/08/04/Wayfire-0-5.html
	 */
	wlr_primary_selection_v1_device_manager_create(server->wl_display);

	server->input_method_manager = wlr_input_method_manager_v2_create(
		server->wl_display);
	server->text_input_manager = wlr_text_input_manager_v3_create(
		server->wl_display);
	seat_init(server);
	xdg_shell_init(server);
	kde_server_decoration_init(server);
	xdg_server_decoration_init(server);

	struct wlr_presentation *presentation =
		wlr_presentation_create(server->wl_display, server->backend);
	if (!presentation) {
		wlr_log(WLR_ERROR, "unable to create presentation interface");
		exit(EXIT_FAILURE);
	}
	wlr_scene_set_presentation(server->scene, presentation);

	wlr_export_dmabuf_manager_v1_create(server->wl_display);
	wlr_screencopy_manager_v1_create(server->wl_display);
	wlr_data_control_manager_v1_create(server->wl_display);
	wlr_viewporter_create(server->wl_display);
	wlr_single_pixel_buffer_manager_v1_create(server->wl_display);
	wlr_fractional_scale_manager_v1_create(server->wl_display,
		LAB_WLR_FRACTIONAL_SCALE_V1_VERSION);

	idle_manager_create(server->wl_display, server->seat.seat);

	server->relative_pointer_manager = wlr_relative_pointer_manager_v1_create(
		server->wl_display);
	server->constraints = wlr_pointer_constraints_v1_create(
		server->wl_display);

	server->new_constraint.notify = create_constraint;
	wl_signal_add(&server->constraints->events.new_constraint,
		&server->new_constraint);

	server->input_inhibit =
		wlr_input_inhibit_manager_create(server->wl_display);
	if (!server->input_inhibit) {
		wlr_log(WLR_ERROR, "unable to create input inhibit manager");
		exit(EXIT_FAILURE);
	}

	wl_signal_add(&server->input_inhibit->events.activate,
		&server->input_inhibit_activate);
	server->input_inhibit_activate.notify = handle_input_inhibit;

	wl_signal_add(&server->input_inhibit->events.deactivate,
		&server->input_inhibit_deactivate);
	server->input_inhibit_deactivate.notify = handle_input_disinhibit;

	server->foreign_toplevel_manager =
		wlr_foreign_toplevel_manager_v1_create(server->wl_display);

	session_lock_init(server);

	server->drm_lease_manager = wlr_drm_lease_v1_manager_create(
		server->wl_display, server->backend);
	if (server->drm_lease_manager) {
		server->drm_lease_request.notify = handle_drm_lease_request;
		wl_signal_add(&server->drm_lease_manager->events.request,
				&server->drm_lease_request);
	} else {
		wlr_log(WLR_DEBUG, "Failed to create wlr_drm_lease_device_v1");
		wlr_log(WLR_INFO, "VR will not be available");
	}

	server->output_power_manager_v1 =
		wlr_output_power_manager_v1_create(server->wl_display);
	server->output_power_manager_set_mode.notify =
		handle_output_power_manager_set_mode;
	wl_signal_add(&server->output_power_manager_v1->events.set_mode,
		&server->output_power_manager_set_mode);

	server->tearing_control = wlr_tearing_control_manager_v1_create(server->wl_display, 1);
	server->tearing_new_object.notify = new_tearing_hint;
	wl_signal_add(&server->tearing_control->events.new_object, &server->tearing_new_object);

	layers_init(server);

#if HAVE_XWAYLAND
	xwayland_server_init(server, compositor);
#endif
}

void
server_start(struct server *server)
{
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server->wl_display);
	if (!socket) {
		wlr_log_errno(WLR_ERROR, "unable to open wayland socket");
		exit(EXIT_FAILURE);
	}

	/*
	 * Start the backend. This will enumerate outputs and inputs, become
	 * the DRM master, etc
	 */
	if (!wlr_backend_start(server->backend)) {
		wlr_log(WLR_ERROR, "unable to start the wlroots backend");
		exit(EXIT_FAILURE);
	}

	/* Potentially set up the initial fallback output */
	output_virtual_update_fallback(server);

	if (setenv("WAYLAND_DISPLAY", socket, true) < 0) {
		wlr_log_errno(WLR_ERROR, "unable to set WAYLAND_DISPLAY");
	} else {
		wlr_log(WLR_DEBUG, "WAYLAND_DISPLAY=%s", socket);
	}
}

void
server_finish(struct server *server)
{
#if HAVE_XWAYLAND
	xwayland_server_finish(server);
#endif
	if (sighup_source) {
		wl_event_source_remove(sighup_source);
	}
	wl_display_destroy_clients(server->wl_display);

	seat_finish(server);
	wlr_output_layout_destroy(server->output_layout);

	wl_display_destroy(server->wl_display);

	/* TODO: clean up various scene_tree nodes */
	workspaces_destroy(server);
}
