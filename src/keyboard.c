// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include "action.h"
#include "key-state.h"
#include "labwc.h"
#include "workspaces.h"

static bool should_cancel_cycling_on_next_key_release;

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

bool
keyboard_any_modifiers_pressed(struct wlr_keyboard *keyboard)
{
	xkb_mod_index_t i;
	for (i = 0; i < xkb_keymap_num_mods(keyboard->keymap); i++) {
		if (xkb_state_mod_index_is_active
				(keyboard->xkb_state, i,
				 XKB_STATE_MODS_DEPRESSED)) {
			return true;
		}
	}
	return false;
}

static void
end_cycling(struct server *server)
{
	desktop_focus_and_activate_view(&server->seat, server->osd_state.cycle_view);
	desktop_move_to_front(server->osd_state.cycle_view);

	/* osd_finish() additionally resets cycle_view to NULL */
	osd_finish(server);
	should_cancel_cycling_on_next_key_release = false;
}

void
keyboard_modifiers_notify(struct wl_listener *listener, void *data)
{
	struct keyboard *keyboard = wl_container_of(listener, keyboard, modifier);
	struct seat *seat = keyboard->base.seat;
	struct server *server = seat->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_keyboard *wlr_keyboard = keyboard->wlr_keyboard;

	if (server->osd_state.cycle_view || seat->workspace_osd_shown_by_modifier) {
		if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED
				&& !keyboard_any_modifiers_pressed(wlr_keyboard))  {
			if (server->osd_state.cycle_view) {
				if (key_state_nr_keys()) {
					should_cancel_cycling_on_next_key_release = true;
				} else {
					end_cycling(server);
				}
			}
			if (seat->workspace_osd_shown_by_modifier) {
				workspaces_osd_hide(seat);
			}
		}
	}
	wlr_seat_keyboard_notify_modifiers(seat->seat, &wlr_keyboard->modifiers);
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
			if (xkb_keysym_to_lower(sym) == keybind->keysyms[i]) {
				key_state_store_pressed_keys_as_bound();
				actions_run(NULL, server, &keybind->actions, 0);
				return true;
			}
		}
	}
	return false;
}

static bool is_modifier_key(xkb_keysym_t sym)
{
	return sym == XKB_KEY_Shift_L ||
		   sym == XKB_KEY_Shift_R ||
		   sym == XKB_KEY_Alt_L ||
		   sym == XKB_KEY_Alt_R ||
		   sym == XKB_KEY_Control_L ||
		   sym == XKB_KEY_Control_R ||
		   sym == XKB_KEY_Super_L ||
		   sym == XKB_KEY_Super_R;
}

static bool
handle_compositor_keybindings(struct keyboard *keyboard,
		struct wlr_keyboard_key_event *event)
{
	struct seat *seat = keyboard->base.seat;
	struct server *server = seat->server;
	struct wlr_keyboard *wlr_keyboard = keyboard->wlr_keyboard;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;

	key_state_set_pressed(event->keycode,
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED);

	/*
	 * Ignore labwc keybindings if input is inhibited
	 * It's important to do this after key_state_set_pressed() to ensure
	 * _all_ key press/releases are registered
	 */
	if (seat->active_client_while_inhibited) {
		return false;
	}

	/*
	 * If a user lets go of the modifier (e.g. alt) before the 'normal' key
	 * (e.g. tab) when window-cycling, we do not end the cycling until both
	 * keys have been released.  If we end the window-cycling on release of
	 * the modifier only, some XWayland clients such as hexchat realise that
	 * tab is pressed (even though we did not forward the event) and because
	 * we absorb the equivalent release event it gets stuck on repeat.
	 */
	if (should_cancel_cycling_on_next_key_release
			&& event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		end_cycling(server);
		handled = true;
		goto out;
	}

	/*
	 * If a press event was handled by a compositor binding, then do not
	 * forward the corresponding release event to clients
	 */
	if (key_state_corresponding_press_event_was_bound(event->keycode)
			&& event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		key_state_bound_key_remove(event->keycode);
		return true;
	}

	uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_keyboard);

	/* Catch C-A-F1 to C-A-F12 to change tty */
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			unsigned int vt = syms[i] - XKB_KEY_XF86Switch_VT_1 + 1;
			if (vt >= 1 && vt <= 12) {
				change_vt(server, vt);
				/*
				 * Don't send any key events to clients when
				 * changing tty
				 */
				handled = true;
				goto out;
			}
		}
	}

	if (server->osd_state.cycle_view) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			for (int i = 0; i < nsyms; i++) {
				if (syms[i] == XKB_KEY_Escape) {
					/*
					 * Cancel view-cycle
					 *
					 * osd_finish() additionally resets
					 * cycle_view to NULL
					 */
					osd_preview_restore(server);
					osd_finish(server);

					handled = true;
					goto out;
				}
			}

			/* cycle to next */
			bool backwards = modifiers & WLR_MODIFIER_SHIFT;
			/* ignore if this is a modifier key being pressed */
			bool ignore = false;
			for (int i = 0; i < nsyms; i++) {
				ignore |= is_modifier_key(syms[i]);
			}

			if (!ignore) {
				enum lab_cycle_dir dir = backwards
					? LAB_CYCLE_DIR_BACKWARD
					: LAB_CYCLE_DIR_FORWARD;
				server->osd_state.cycle_view = desktop_cycle_view(server,
					server->osd_state.cycle_view, dir);
				osd_update(server);
			}
		}
		/* don't send any key events to clients when osd onscreen */
		handled = true;
		goto out;
	}

	/* Handle compositor key bindings */
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			handled |= handle_keybinding(server, modifiers, syms[i]);
		}
	}

out:
	if (handled) {
		key_state_store_pressed_keys_as_bound();
	}

	return handled;
}

static int
handle_keybind_repeat(void *data)
{
	struct keyboard *keyboard = data;
	assert(keyboard->keybind_repeat);

	/* synthesize event */
	struct wlr_keyboard_key_event event = {
		.keycode = keyboard->keybind_repeat_keycode,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED
	};

	handle_compositor_keybindings(keyboard, &event);
	wl_event_source_timer_update(keyboard->keybind_repeat,
		keyboard->keybind_repeat_rate);

	return 0; /* ignored per wl_event_loop docs */
}

static void
start_keybind_repeat(struct server *server, struct keyboard *keyboard,
		struct wlr_keyboard_key_event *event)
{
	struct wlr_keyboard *wlr_keyboard = keyboard->wlr_keyboard;
	assert(!keyboard->keybind_repeat);

	if (wlr_keyboard->repeat_info.rate > 0
			&& wlr_keyboard->repeat_info.delay > 0) {
		keyboard->keybind_repeat_keycode = event->keycode;
		keyboard->keybind_repeat_rate = wlr_keyboard->repeat_info.rate;
		keyboard->keybind_repeat = wl_event_loop_add_timer(
			server->wl_event_loop, handle_keybind_repeat, keyboard);
		wl_event_source_timer_update(keyboard->keybind_repeat,
			wlr_keyboard->repeat_info.delay);
	}
}

void
keyboard_cancel_keybind_repeat(struct keyboard *keyboard)
{
	if (keyboard->keybind_repeat) {
		wl_event_source_remove(keyboard->keybind_repeat);
		keyboard->keybind_repeat = NULL;
	}
}

void
keyboard_key_notify(struct wl_listener *listener, void *data)
{
	/* This event is raised when a key is pressed or released. */
	struct keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct seat *seat = keyboard->base.seat;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *wlr_seat = seat->seat;
	struct wlr_keyboard *wlr_keyboard = keyboard->wlr_keyboard;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	/* any new press/release cancels current keybind repeat */
	keyboard_cancel_keybind_repeat(keyboard);

	bool handled = handle_compositor_keybindings(keyboard, event);
	if (handled) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			start_keybind_repeat(seat->server, keyboard, event);
		}
	} else {
		wlr_seat_set_keyboard(wlr_seat, wlr_keyboard);
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
	if (keymap) {
		wlr_keyboard_set_keymap(kb, keymap);
		xkb_keymap_unref(keymap);
	} else {
		wlr_log(WLR_ERROR, "Failed to create xkb keymap");
	}
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(kb, rc.repeat_rate, rc.repeat_delay);
}

void
keyboard_finish(struct seat *seat)
{
	/*
	 * All keyboard listeners must be removed before this to avoid use after
	 * free
	 */
	if (seat->keyboard_group) {
		wlr_keyboard_group_destroy(seat->keyboard_group);
		seat->keyboard_group = NULL;
	}
}
