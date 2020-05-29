#include "labwc.h"

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client.
	 */
	struct keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to
	 * the same seat. You can swap out the underlying wlr_keyboard like
	 * this and wlr_seat handles this transparently.
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
		server->cycle_view = next_toplevel(view_front_toplevel(server));
		fprintf(stderr, "cycle_view=%p\n", (void *)server->cycle_view);
		break;
	case XKB_KEY_F3:
		if (fork() == 0) {
			execl("/bin/dmenu_run", "/bin/dmenu_run", (void *)NULL);
		}
		break;
	case XKB_KEY_F6:
		interactive_begin(view_front_toplevel(server),
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

	if (server->cycle_view) {
		if ((syms[0] == XKB_KEY_Alt_L) &&
		    event->state == WLR_KEY_RELEASED) {
			/* end cycle */
			view_focus(server->cycle_view);
			server->cycle_view = NULL;
		} else if (event->state == WLR_KEY_PRESSED) {
			/* cycle to next */
			server->cycle_view = next_toplevel(server->cycle_view);
			fprintf(stderr, "cycle_view=%p\n",
				(void *)server->cycle_view);
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

	/*
	 * We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us").
	 */
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
	/* TODO: Configure libinput on device to set tap, acceleration, etc */
	wlr_cursor_attach_input_device(server->cursor, device);
}

void server_new_input(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised by the backend when a new input device becomes
	 * available.
	 */
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

	/*
	 * We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client.
	 */
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
	/*
	 * This event is rasied by the seat when a client provides a cursor
	 * image
	 */
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
