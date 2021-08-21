#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include "labwc.h"

static void
change_vt(struct server *server, unsigned int vt)
{
	if (!wlr_backend_is_multi(server->backend)) {
		return;
	}
	struct wlr_session *session = wlr_backend_get_session(server->backend);
	if (session) {
		wlr_session_change_vt(session, vt);
	}
}

static void
keyboard_modifiers_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, keyboard_modifiers);
	wlr_seat_keyboard_notify_modifiers(seat->seat,
		&seat->keyboard_group->keyboard.modifiers);
}

static bool
handle_keybinding(struct server *server, uint32_t modifiers, xkb_keysym_t sym)
{
	struct keybind *keybind;
	wl_list_for_each_reverse (keybind, &rc.keybinds, link) {
		if (modifiers ^ keybind->modifiers) {
			continue;
		}
		for (size_t i = 0; i < keybind->keysyms_len; i++) {
			if (sym == keybind->keysyms[i]) {
				action(server, keybind->action,
				       keybind->command);
				return true;
			}
		}
	}
	return false;
}

static bool
handle_compositor_keybindings(struct wl_listener *listener,
			      struct wlr_event_keyboard_key *event)
{
	struct seat *seat = wl_container_of(listener, seat, keyboard_key);
	struct server *server = seat->server;
	struct wlr_input_device *device = seat->keyboard_group->input_device;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(device->keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers =
		wlr_keyboard_get_modifiers(device->keyboard);

	if (server->cycle_view) {
		damage_all_outputs(server);
		if ((syms[0] == XKB_KEY_Alt_L) &&
		    event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
			/* end cycle */
			desktop_focus_view(&server->seat, server->cycle_view);
			server->cycle_view = NULL;
			/* XXX should we handled=true here? */
		} else if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			/* cycle to next */
			server->cycle_view =
				desktop_cycle_view(server, server->cycle_view);
			osd_update(server);
			damage_all_outputs(server);
			return true;
		}
	}

	/* Handle compositor key bindings */
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, modifiers, syms[i]);
		}
	}

	/* Catch C-A-F1 to C-A-F12 to change tty */
	if (!handled && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			unsigned int vt = syms[i] - XKB_KEY_XF86Switch_VT_1 + 1;
			if (vt >= 1 && vt <= 12) {
				change_vt(server, vt);
				handled = true;
			}
		}
	}
	return handled;
}
static void
keyboard_key_notify(struct wl_listener *listener, void *data)
{
        /* XXX need to check if input inhibited before doing any
	 * compositor bindings
	 */

	/* This event is raised when a key is pressed or released. */
	struct seat *seat = wl_container_of(listener, seat, keyboard_key);
	struct server *server = seat->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *wlr_seat = server->seat.seat;
	struct wlr_input_device *device = seat->keyboard_group->input_device;

	bool handled = false;

	if(!seat->active_client_while_inhibited)
		/* ignore labwc keybindings if input is inhibited */
		handled = handle_compositor_keybindings(listener, event);

	if (!handled) {
		wlr_seat_set_keyboard(wlr_seat, device);
		wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
					     event->keycode, event->state);
	}
}

void
keyboard_init(struct seat *seat)
{
	seat->keyboard_group = wlr_keyboard_group_create();
	struct wlr_keyboard *kb = &seat->keyboard_group->keyboard;
	struct xkb_rule_names rules = { 0 };
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);
	wlr_keyboard_set_keymap(kb, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(kb, 25, 600);

	seat->keyboard_key.notify = keyboard_key_notify;
	wl_signal_add(&kb->events.key, &seat->keyboard_key);
	seat->keyboard_modifiers.notify = keyboard_modifiers_notify;
	wl_signal_add(&kb->events.modifiers, &seat->keyboard_modifiers);
}
