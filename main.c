#include "labwc.h"

#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>

int main(int argc, char *argv[])
{
	wlr_log_init(WLR_ERROR, NULL);
	char *startup_cmd = NULL;

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

	struct server server;
	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	server.wl_display = wl_display_create();
	/* The backend is a wlroots feature which abstracts the underlying input
	 * and output hardware. The autocreate option will choose the most
	 * suitable backend based on the current environment, such as opening an
	 * X11 window if an X11 server is running. The NULL argument here
	 * optionally allows you to pass in a custom renderer if wlr_renderer
	 * doesn't meet your needs. The backend uses the renderer, for example,
	 * to fall back to software cursors if the backend does not support
	 * hardware cursors (some older GPUs don't). */
	server.backend = wlr_backend_autocreate(server.wl_display, NULL);

	/* If we don't provide a renderer, autocreate makes a GLES2 renderer for
	 * us. The renderer is responsible for defining the various pixel
	 * formats it supports for shared memory, this configures that for
	 * clients. */
	server.renderer = wlr_backend_get_renderer(server.backend);
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device
	 * manager handles the clipboard. Each of these wlroots interfaces has
	 * room for you to dig your fingers in and play with their behavior if
	 * you want. */
	server.compositor =
		wlr_compositor_create(server.wl_display, server.renderer);
	wlr_data_device_manager_create(server.wl_display);

	wlr_export_dmabuf_manager_v1_create(server.wl_display);
	wlr_screencopy_manager_v1_create(server.wl_display);
	wlr_data_control_manager_v1_create(server.wl_display);
	wlr_gamma_control_manager_v1_create(server.wl_display);
	wlr_primary_selection_v1_device_manager_create(server.wl_display);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	server.output_layout = wlr_output_layout_create();

	/* Configure a listener to be notified when new outputs are available on
	 * the backend. */
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	/* Set up our list of views and the xdg-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to my article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&server.views);
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display);
	server.new_xdg_surface.notify = xdg_surface_new;
	wl_signal_add(&server.xdg_shell->events.new_surface,
		      &server.new_xdg_surface);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move
	 * around when the pointer moves. However, we can attach input devices
	 * to it, and it will generate aggregate events for all of them. In
	 * these events, we can choose how we want to process them, forwarding
	 * them to clients and moving the cursor around. More detail on this
	 * process is described in my input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions
	 * above.
	 */
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

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener
	 * to let us know when new input devices are available on the backend.
	 */
	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	server.seat = wlr_seat_create(server.wl_display, "seat0");
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
		      &server.request_cursor);

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

	/* Start the backend. This will enumerate outputs and inputs, become the
	 * DRM master, etc */
	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run
	 * the startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);

	wl_display_init_shm(server.wl_display);

	/* Init xwayland */
	server.xwayland = wlr_xwayland_create(server.wl_display,
					      server.compositor, false);
	server.new_xwayland_surface.notify = xwl_surface_new;
	wl_signal_add(&server.xwayland->events.new_surface,
		      &server.new_xwayland_surface);
	setenv("DISPLAY", server.xwayland->display_name, true);
	wlr_xwayland_set_seat(server.xwayland, server.seat);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that
	 * cursor images are available at all scale factors on the screen
	 * (necessary for
	 * HiDPI support). We add a cursor theme at scale factor 1 to begin
	 * with. */
	server.cursor_mgr =
		wlr_xcursor_manager_create(XCURSOR_DEFAULT, XCURSOR_SIZE);
	wlr_xcursor_manager_load(server.cursor_mgr, 1);

	struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
		server.cursor_mgr, XCURSOR_DEFAULT, 1);
	if (xcursor) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server.xwayland, image->buffer,
					image->width * 4, image->width,
					image->height, image->hotspot_x,
					image->hotspot_y);
	}

	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd,
			      (void *)NULL);
		}
	}
	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
		socket);
	wl_display_run(server.wl_display);

	/* Once wl_display_run returns, we shut down the server. */
	wlr_xwayland_destroy(server.xwayland);
	wlr_xcursor_manager_destroy(server.cursor_mgr);
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
