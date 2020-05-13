#include "labwc.h"

#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>

int main(int argc, char *argv[])
{
	struct server server = { 0 };
	char *startup_cmd = NULL;

	wlr_log_init(WLR_ERROR, NULL);

	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	/* Wayland requires XDG_RUNTIME_DIR to be set */
	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is not set");
		return 1;
	}

	/*
	 * The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on.
	 */
	server.wl_display = wl_display_create();
	if (!server.wl_display) {
		wlr_log(WLR_ERROR, "cannot allocate a wayland display");
		return 1;
	}

	/* TODO: Catch signals. Maybe SIGHUP for reconfigure */

	/*
	 * The backend is a wlroots feature which abstracts the underlying
	 * input and output hardware. the autocreate option will choose the
	 * most suitable backend based on the current environment, such as
	 * opening an x11 window if an x11 server is running. the null
	 * argument here optionally allows you to pass in a custom renderer if
	 * wlr_renderer doesn't meet your needs. the backend uses the
	 * renderer, for example, to fall back to software cursors if the
	 * backend does not support hardware cursors (some older gpus don't).
	 */
	server.backend = wlr_backend_autocreate(server.wl_display, NULL);
	if (!server.backend) {
		wlr_log(WLR_ERROR, "unable to create the wlroots backend");
		return 1;
	}

	/*
	 * If we don't provide a renderer, autocreate makes a GLES2 renderer
	 * for us. The renderer is responsible for defining the various pixel
	 * formats it supports for shared memory, this configures that for
	 * clients.
	 */
	server.renderer = wlr_backend_get_renderer(server.backend);
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	wl_list_init(&server.views);
	wl_list_init(&server.outputs);

	/*
	 * Create an output layout, which a wlroots utility for working with
	 * an arrangement of screens in a physical layout.
	 */
	server.output_layout = wlr_output_layout_create();
	if (!server.output_layout) {
		wlr_log(WLR_ERROR, "unable to create output layout");
		return 1;
	}

	/*
	 * Create some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device
	 * manager handles the clipboard. Each of these wlroots interfaces has
	 * room for you to dig your fingers in and play with their behavior if
	 * you want.
	 */
	server.compositor =
		wlr_compositor_create(server.wl_display, server.renderer);
	if (!server.compositor) {
		wlr_log(WLR_ERROR, "unable to create the wlroots compositor");
		return 1;
	}

	struct wlr_data_device_manager *device_manager = NULL;
	device_manager = wlr_data_device_manager_create(server.wl_display);
	if (!device_manager) {
		wlr_log(WLR_ERROR, "unable to create data device manager");
		return 1;
	}

	/*
	 * Configure a listener to be notified when new outputs are available
	 * on the backend.
	 */
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits
	 * and operates the computer. This conceptually includes up to one
	 * keyboard, pointer, touch, and drawing tablet device. We also rig up
	 * a listener to let us know when new input devices are available on
	 * the backend.
	 */
	server.seat = wlr_seat_create(server.wl_display, "seat0");
	if (!server.seat) {
		wlr_log(WLR_ERROR, "cannot allocate seat0");
		return 1;
	}

	server.cursor = wlr_cursor_create();
	if (!server.cursor) {
		wlr_log(WLR_ERROR, "unable to create cursor");
		return 1;
	}
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
	server.cursor_mgr = wlr_xcursor_manager_create(NULL, XCURSOR_SIZE);
	if (!server.cursor_mgr) {
		wlr_log(WLR_ERROR, "cannot create xcursor manager");
		return 1;
	}

	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute,
		      &server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
		      &server.request_cursor);
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection,
		      &server.request_set_selection);

	/* Init xdg-shell */
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display);
	if (!server.xdg_shell) {
		wlr_log(WLR_ERROR, "unable to create the XDG shell interface");
		return 1;
	}
	server.new_xdg_surface.notify = xdg_surface_new;
	wl_signal_add(&server.xdg_shell->events.new_surface,
		      &server.new_xdg_surface);

	/* Disable CSD */
	struct wlr_xdg_decoration_manager_v1 *xdg_deco_mgr = NULL;
	xdg_deco_mgr = wlr_xdg_decoration_manager_v1_create(server.wl_display);
	if (!xdg_deco_mgr) {
		wlr_log(WLR_ERROR, "unable to create the XDG deco manager");
		return 1;
	}
	wl_signal_add(&xdg_deco_mgr->events.new_toplevel_decoration,
		      &server.xdg_toplevel_decoration);
	server.xdg_toplevel_decoration.notify = xdg_toplevel_decoration;

	struct wlr_server_decoration_manager *deco_mgr = NULL;
	deco_mgr = wlr_server_decoration_manager_create(server.wl_display);
	if (!deco_mgr) {
		wlr_log(WLR_ERROR, "unable to create the server deco manager");
		return 1;
	}
	wlr_server_decoration_manager_set_default_mode(
		deco_mgr, LAB_DISABLE_CSD ?
				  WLR_SERVER_DECORATION_MANAGER_MODE_SERVER :
				  WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);

	/* FIXME: Check return values */
	wlr_export_dmabuf_manager_v1_create(server.wl_display);
	wlr_screencopy_manager_v1_create(server.wl_display);
	wlr_data_control_manager_v1_create(server.wl_display);
	wlr_gamma_control_manager_v1_create(server.wl_display);
	wlr_primary_selection_v1_device_manager_create(server.wl_display);

	/* Init xwayland */
	server.xwayland = wlr_xwayland_create(server.wl_display,
					      server.compositor, false);
	if (!server.xwayland) {
		wlr_log(WLR_ERROR, "cannot create xwayland server");
		return 1;
	}
	server.new_xwayland_surface.notify = xwl_surface_new;
	wl_signal_add(&server.xwayland->events.new_surface,
		      &server.new_xwayland_surface);

	server.cursor_mgr =
		wlr_xcursor_manager_create(XCURSOR_DEFAULT, XCURSOR_SIZE);
	if (!server.cursor_mgr) {
		wlr_log(WLR_ERROR, "cannot create xwayland xcursor manager");
		return 1;
	}

	if (setenv("DISPLAY", server.xwayland->display_name, true) < 0)
		wlr_log_errno(WLR_ERROR, "unable to set DISPLAY for xwayland");
	else
		wlr_log(WLR_DEBUG, "xwayland is running on display %s",
			server.xwayland->display_name);

	if (wlr_xcursor_manager_load(server.cursor_mgr, 1))
		wlr_log(WLR_ERROR, "cannot load xwayland xcursor theme");

	struct wlr_xcursor *xcursor;
	xcursor = wlr_xcursor_manager_get_xcursor(server.cursor_mgr,
						  XCURSOR_DEFAULT, 1);
	if (xcursor) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server.xwayland, image->buffer,
					image->width * 4, image->width,
					image->height, image->hotspot_x,
					image->hotspot_y);
	}

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_log_errno(WLR_ERROR, "unable to open wayland socket");
		return 1;
	}

	/*
	 * Start the backend. This will enumerate outputs and inputs, become
	 * the DRM master, etc
	 */
	if (!wlr_backend_start(server.backend)) {
		wlr_log(WLR_ERROR, "unable to start the wlroots backend");
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, true);
	if (setenv("WAYLAND_DISPLAY", socket, true) < 0)
		wlr_log_errno(WLR_ERROR, "unable to set WAYLAND_DISPLAY");
	else
		wlr_log(WLR_DEBUG, "WAYLAND_DISPLAY=%s", socket);

	wl_display_init_shm(server.wl_display);

	wlr_xwayland_set_seat(server.xwayland, server.seat);

	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd,
			      (void *)NULL);
		}
	}

	wl_display_run(server.wl_display);

	wlr_xwayland_destroy(server.xwayland);
	wlr_xcursor_manager_destroy(server.cursor_mgr);
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
