#include "labwc.h"

static bool in_alt_tab_mode;
static struct view *alt_tab_view;

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to
	 * the same seat. You can swap out the underlying wlr_keyboard like this
	 * and wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(
		keyboard->server->seat, &keyboard->device->keyboard->modifiers);
}

static bool handle_keybinding(struct server *server, xkb_keysym_t sym)
{
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its
	 * own processing.
	 *
	 * This function assumes Alt is held down.
	 */
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_F1:
	case XKB_KEY_F2:
		in_alt_tab_mode = true;
		alt_tab_view = next_toplevel(view_front_toplevel(server));
		fprintf(stderr, "alt_tab_view=%p\n", (void *)alt_tab_view);
		break;
	case XKB_KEY_F3:
		if (fork() == 0) {
			execl("/bin/dmenu_run", "/bin/dmenu_run", (void *)NULL);
		}
		break;
	case XKB_KEY_F6:
		begin_interactive(view_front_toplevel(server),
				  TINYWL_CURSOR_MOVE, 0);
		break;
	case XKB_KEY_F12:
		dbg_show_views(server);
		break;
	default:
		return false;
	}
	return true;
}

static void keyboard_handle_key(struct wl_listener *listener, void *data)
{
	/* This event is raised when a key is pressed or released. */
	struct keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
		keyboard->device->keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers =
		wlr_keyboard_get_modifiers(keyboard->device->keyboard);

	if (in_alt_tab_mode) {
		if ((syms[0] == XKB_KEY_Alt_L) &&
		    event->state == WLR_KEY_RELEASED) {
			/* end cycle */
			in_alt_tab_mode = false;
			view_focus(alt_tab_view);
		} else if (event->state == WLR_KEY_PRESSED) {
			/* cycle to next */
			alt_tab_view = next_toplevel(alt_tab_view);
			fprintf(stderr, "alt_tab_view=%p\n",
				(void *)alt_tab_view);
			return;
		}
	}

	/* Handle compositor key bindings */
	if ((modifiers & WLR_MODIFIER_ALT) && event->state == WLR_KEY_PRESSED) {
		/* If alt is held down and this button was _pressed_, we attempt
		 * to process it as a compositor keybinding. */
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
		}
	}
	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
					     event->keycode, event->state);
	}
}

static void server_new_keyboard(struct server *server,
				struct wlr_input_device *device)
{
	struct keyboard *keyboard = calloc(1, sizeof(struct keyboard));
	keyboard->server = server;
	keyboard->device = device;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_rule_names rules = { 0 };
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(
		context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(device->keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&device->keyboard->events.modifiers,
		      &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);

	wlr_seat_set_keyboard(server->seat, device);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct server *server,
			       struct wlr_input_device *device)
{
	/* We don't do anything special with pointers. All of our pointer
	 * handling is proxied through wlr_cursor. On another compositor, you
	 * might take this opportunity to do libinput configuration on the
	 * device to set acceleration, etc. */
	wlr_cursor_attach_input_device(server->cursor, device);
}

void server_new_input(struct wl_listener *listener, void *data)
{
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
		break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In TinyWL we always have a cursor, even
	 * if there are no pointer devices, so we always include that
	 * capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

void seat_request_cursor(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, request_cursor);
	/* This event is rasied by the seat when a client provides a cursor
	 * image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use
		 * the provided surface as the cursor image. It will set the
		 * hardware cursor on the output that it's currently on and
		 * continue to do so as the cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface,
				       event->hotspot_x, event->hotspot_y);
	}
}

void seat_request_set_selection(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void process_cursor_move(struct server *server, uint32_t time)
{
	/* Move the grabbed view to the new position. */
	server->grabbed_view->x = server->cursor->x - server->grab_x;
	server->grabbed_view->y = server->cursor->y - server->grab_y;

	struct view *view = server->grabbed_view;
	if (view->type == LAB_XWAYLAND_VIEW) {
		wlr_xwayland_surface_configure(view->xwayland_surface,
					       server->grabbed_view->x,
					       server->grabbed_view->y,
					       view->xwayland_surface->width,
					       view->xwayland_surface->height);
	}
}

static void process_cursor_resize(struct server *server, uint32_t time)
{
	/*
	 * Resizing the grabbed view can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the
	 * view on one or two axes, but can also move the view if you resize
	 * from the top or left edges (or top-left corner).
	 *
	 * TODO: Wait for the client to prepare a buffer at the new size, then
	 * commit any movement that was prepared.
	 */
	struct view *view = server->grabbed_view;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_box.x;
	int new_right = server->grab_box.x + server->grab_box.width;
	int new_top = server->grab_box.y;
	int new_bottom = server->grab_box.y + server->grab_box.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom)
			new_top = new_bottom - 1;
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top)
			new_bottom = new_top + 1;
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right)
			new_left = new_right - 1;
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left)
			new_right = new_left + 1;
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
	view->x = new_left - geo_box.x;
	view->y = new_top - geo_box.y;

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(view->xdg_surface, new_width, new_height);
}

static void process_cursor_motion(struct server *server, uint32_t time)
{
	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->cursor_mode == TINYWL_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == TINYWL_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Otherwise, find the view under the pointer and send the event along.
	 */
	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	int view_area;
	struct view *view = view_at(server, server->cursor->x,
				    server->cursor->y, &surface, &sx, &sy,
				    &view_area);
	if (!view) {
		/* If there's no view under the cursor, set the cursor image to
		 * a default. This is what makes the cursor image appear when
		 * you move it around the screen, not over any views. */
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, "left_ptr", server->cursor);
	}
	switch (view_area) {
	case LAB_DECO_PART_TOP:
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, "left_ptr", server->cursor);
		break;
	}
	if (surface) {
		bool focus_changed = seat->pointer_state.focused_surface !=
				     surface;
		/*
		 * "Enter" the surface if necessary. This lets the client know
		 * that the cursor has entered one of its surfaces.
		 *
		 * Note that this gives the surface "pointer focus", which is
		 * distinct from keyboard focus. You get pointer focus by moving
		 * the pointer over a window.
		 */
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		if (!focus_changed) {
			/* The enter event contains coordinates, so we only need
			 * to notify on motion if the focus did not change. */
			wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		}
	} else {
		/* Clear pointer focus so future button events and such are not
		 * sent to the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}

void server_cursor_motion(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta) */
	struct server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor
	 * automatically handles constraining the motion to the output layout,
	 * as well as any special configuration applied for the specific input
	 * device which generated the event. You can pass NULL for the device if
	 * you want to move the cursor around without any input. */
	wlr_cursor_move(server->cursor, event->device, event->delta_x,
			event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an
	 * _absolute_ motion event, from 0..1 on each axis. This happens, for
	 * example, when wlroots is running under a Wayland window rather than
	 * KMS+DRM, and you move the mouse over the window. You could enter the
	 * window from any edge, so we have to warp the mouse there. There is
	 * also some hardware which emits these events. */
	struct server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(server->cursor, event->device, event->x,
				 event->y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_button(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_event_pointer_button *event = data;
	/* Notify the client with pointer focus that a button press has occurred
	 */
	wlr_seat_pointer_notify_button(server->seat, event->time_msec,
				       event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface;
	int view_area;
	struct view *view = view_at(server, server->cursor->x,
				    server->cursor->y, &surface, &sx, &sy,
				    &view_area);
	if (event->state == WLR_BUTTON_RELEASED) {
		/* If you released any buttons, we exit interactive move/resize
		 * mode. */
		server->cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
	} else {
		/* Focus that client if the button was _pressed_ */
		view_focus(view);
		switch (view_area) {
		case LAB_DECO_PART_TOP:
			begin_interactive(view, TINYWL_CURSOR_MOVE, 0);
			break;
		}
	}
}

void server_cursor_axis(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an axis
	 * event, for example when you move the scroll wheel. */
	struct server *server = wl_container_of(listener, server, cursor_axis);
	struct wlr_event_pointer_axis *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(server->seat, event->time_msec,
				     event->orientation, event->delta,
				     event->delta_discrete, event->source);
}

void server_cursor_frame(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at
	 * the
	 * same time, in which case a frame event won't be sent in between. */
	struct server *server = wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

void server_new_output(struct wl_listener *listener, void *data)
{
	/* This event is rasied by the backend when a new output (aka a display
	 * or monitor) becomes available. */
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/*
	 * Some backends don't have modes. DRM+KMS does, and we need to set a
	 * mode before we can use the output. The mode is a tuple of (width,
	 * height, refresh rate), and each monitor supports only a specific set
	 * of modes. We just pick the monitor's preferred mode.
	 * TODO: support user configuration
	 */
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
	}

	/* Allocates and configures our state for this output */
	struct output *output = calloc(1, sizeof(struct output));
	output->wlr_output = wlr_output;
	output->server = server;
	/* Sets up a listener for the frame notify event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);

	/* Adds this to the output layout. The add_auto function arranges
	 * outputs from left-to-right in the order they appear. A more
	 * sophisticated compositor would let the user configure the arrangement
	 * of outputs in the layout.
	 *
	 * The output layout utility automatically adds a wl_output global to
	 * the display, which Wayland clients can see to find out information
	 * about the output (such as DPI, scale factor, manufacturer, etc).
	 */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}
