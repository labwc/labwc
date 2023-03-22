// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <signal.h>
#include <sys/wait.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_viewporter.h>
#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include "drm-lease-v1-protocol.h"
#include "config/rcxml.h"
#include "config/session.h"
#include "decorations.h"
#include "labwc.h"
#include "layers.h"
#include "menu/menu.h"
#include "regions.h"
#include "theme.h"
#include "view.h"
#include "workspaces.h"
#include "xwayland.h"

#define LAB_XDG_SHELL_VERSION (2)

static struct wlr_compositor *compositor;
static struct wl_event_source *sighup_source;
static struct wl_event_source *sigint_source;
static struct wl_event_source *sigterm_source;

static struct server *g_server;

static void
reload_config_and_theme(void)
{
	rcxml_finish();
	rcxml_read(NULL);
	theme_finish(g_server->theme);
	theme_init(g_server->theme, rc.theme_name);

	struct view *view;
	wl_list_for_each(view, &g_server->views, link) {
		view_reload_ssd(view);
	}

	menu_reconfigure(g_server);
	seat_reconfigure(g_server);
	regions_reconfigure(g_server);
	kde_server_decoration_update_default();
}

static int
handle_sighup(int signal, void *data)
{
	session_environment_init(rc.config_dir);
	reload_config_and_theme();
	return 0;
}

static int
handle_sigterm(int signal, void *data)
{
	struct wl_display *display = data;

	wl_display_terminate(display);
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
	struct wl_client *xwayland_client =
		server->xwayland ? server->xwayland->server->client : NULL;
	if (xwayland_client && client == xwayland_client) {
		/*
		 * Filter out wp_drm_lease_device_v1 for now as it is resulting in
		 * issues with Xwayland applications lagging over time.
		 *
		 * https://github.com/labwc/labwc/issues/553
		 */
		if (!strcmp(iface->name, wp_drm_lease_device_v1_interface.name)) {
			return false;
		}
	}
#endif

	return true;
}

void
server_init(struct server *server)
{
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
		event_loop, SIGHUP, handle_sighup, NULL);
	sigint_source = wl_event_loop_add_signal(
		event_loop, SIGINT, handle_sigterm, server->wl_display);
	sigterm_source = wl_event_loop_add_signal(
		event_loop, SIGTERM, handle_sigterm, server->wl_display);
	server->wl_event_loop = event_loop;

	/*
	 * The backend is a feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an x11
	 * window if an x11 server is running.
	 */
	server->backend = wlr_backend_autocreate(server->wl_display);
	if (!server->backend) {
		wlr_log(WLR_ERROR, "unable to create backend");
		exit(EXIT_FAILURE);
	}

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
	server->view_tree = wlr_scene_tree_create(&server->scene->tree);
	server->view_tree_always_on_top = wlr_scene_tree_create(&server->scene->tree);
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
	compositor =
		wlr_compositor_create(server->wl_display, server->renderer);
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

	seat_init(server);

	/* Init xdg-shell */
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display,
		LAB_XDG_SHELL_VERSION);
	if (!server->xdg_shell) {
		wlr_log(WLR_ERROR, "unable to create the XDG shell interface");
		exit(EXIT_FAILURE);
	}
	server->new_xdg_surface.notify = xdg_surface_new;
	wl_signal_add(&server->xdg_shell->events.new_surface,
		&server->new_xdg_surface);

	kde_server_decoration_init(server);
	xdg_server_decoration_init(server);

	server->xdg_activation = wlr_xdg_activation_v1_create(server->wl_display);
	if (!server->xdg_activation) {
		wlr_log(WLR_ERROR, "unable to create xdg_activation interface");
		exit(EXIT_FAILURE);
	}
	server->xdg_activation_request.notify = xdg_activation_handle_request;
	wl_signal_add(&server->xdg_activation->events.request_activate,
		&server->xdg_activation_request);

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
	wlr_gamma_control_manager_v1_create(server->wl_display);
	wlr_viewporter_create(server->wl_display);
	wlr_single_pixel_buffer_manager_v1_create(server->wl_display);

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

	layers_init(server);

#if HAVE_XWAYLAND
	xwayland_server_init(server, compositor);
#endif
	/* used when handling SIGHUP */
	g_server = server;
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
