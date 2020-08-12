#include "labwc.h"
#include "common/log.h"

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

static bool handle_keybinding(struct server *server, uint32_t modifiers,
			      xkb_keysym_t sym)
{
	struct keybind *keybind;
	wl_list_for_each_reverse (keybind, &rc.keybinds, link) {
		if (modifiers ^ keybind->modifiers)
			continue;
		for (size_t i = 0; i < keybind->keysyms_len; i++) {
			if (sym == keybind->keysyms[i]) {
				action(server, keybind);
				return true;
			}
		}
	}
	return false;
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
			info("cycle_view=%p", (void *)server->cycle_view);
			return;
		}
	}

	/* Handle compositor key bindings */
	if (event->state == WLR_KEY_PRESSED)
		for (int i = 0; i < nsyms; i++)
			handled = handle_keybinding(server, modifiers, syms[i]);

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
					     event->keycode, event->state);
	}
}

void keyboard_new(struct server *server, struct wlr_input_device *device)
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
