// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include "action.h"
#include "idle.h"
#include "input/keyboard.h"
#include "input/key-state.h"
#include "labwc.h"
#include "menu/menu.h"
#include "regions.h"
#include "view.h"
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
		if (xkb_state_mod_index_is_active(keyboard->xkb_state,
				i, XKB_STATE_MODS_DEPRESSED)) {
			return true;
		}
	}
	return false;
}

static void
end_cycling(struct server *server)
{
	if (server->osd_state.cycle_view) {
		desktop_focus_view(server->osd_state.cycle_view,
			/*raise*/ true);
	}

	/* osd_finish() additionally resets cycle_view to NULL */
	osd_finish(server);
	should_cancel_cycling_on_next_key_release = false;
}

static void
keyboard_modifiers_notify(struct wl_listener *listener, void *data)
{
	struct keyboard *keyboard = wl_container_of(listener, keyboard, modifier);
	struct seat *seat = keyboard->base.seat;
	struct server *server = seat->server;
	struct wlr_keyboard *wlr_keyboard = keyboard->wlr_keyboard;

	if (server->input_mode == LAB_INPUT_STATE_MOVE) {
		/* Any change to the modifier state re-enable region snap */
		seat->region_prevent_snap = false;
	}

	if (server->osd_state.cycle_view || server->grabbed_view
			|| seat->workspace_osd_shown_by_modifier) {
		if (!keyboard_any_modifiers_pressed(wlr_keyboard))  {
			if (server->osd_state.cycle_view) {
				if (key_state_nr_bound_keys()) {
					should_cancel_cycling_on_next_key_release = true;
				} else {
					end_cycling(server);
				}
			}
			if (seat->workspace_osd_shown_by_modifier) {
				workspaces_osd_hide(seat);
			}
			if (server->grabbed_view) {
				regions_hide_overlay(seat);
			}
		}
	}
	wlr_seat_keyboard_notify_modifiers(seat->seat, &wlr_keyboard->modifiers);
}

static bool
handle_keybinding(struct server *server, uint32_t modifiers, xkb_keysym_t sym, xkb_keycode_t code)
{
	struct keybind *keybind;
	wl_list_for_each(keybind, &rc.keybinds, link) {
		if (modifiers ^ keybind->modifiers) {
			continue;
		}
		if (server->seat.nr_inhibited_keybind_views
				&& server->focused_view
				&& server->focused_view->inhibits_keybinds
				&& !actions_contain_toggle_keybinds(&keybind->actions)) {
			continue;
		}
		if (sym == XKB_KEY_NoSymbol) {
			/* Use keycodes */
			for (size_t i = 0; i < keybind->keycodes_len; i++) {
				if (keybind->keycodes[i] == code) {
					actions_run(NULL, server, &keybind->actions, 0);
					return true;
				}
			}
		} else {
			/* Use syms */
			for (size_t i = 0; i < keybind->keysyms_len; i++) {
				if (xkb_keysym_to_lower(sym) == keybind->keysyms[i]) {
					actions_run(NULL, server, &keybind->actions, 0);
					return true;
				}
			}
		}
	}
	return false;
}

static bool
is_modifier_key(xkb_keysym_t sym)
{
	switch (sym) {
	case XKB_KEY_Shift_L:   case XKB_KEY_Shift_R:
	case XKB_KEY_Alt_L:     case XKB_KEY_Alt_R:
	case XKB_KEY_Control_L: case XKB_KEY_Control_R:
	case XKB_KEY_Super_L:   case XKB_KEY_Super_R:
	case XKB_KEY_Hyper_L:   case XKB_KEY_Hyper_R:
	case XKB_KEY_Meta_L:    case XKB_KEY_Meta_R:
	case XKB_KEY_Mode_switch:
	case XKB_KEY_ISO_Level3_Shift:
	case XKB_KEY_ISO_Level5_Shift:
		return true;
	default:
		return false;
	}
}

struct keysyms {
	const xkb_keysym_t *syms;
	int nr_syms;
};

static void
handle_menu_keys(struct server *server, struct keysyms *syms)
{
	assert(server->input_mode == LAB_INPUT_STATE_MENU);

	for (int i = 0; i < syms->nr_syms; i++) {
		switch (syms->syms[i]) {
		case XKB_KEY_Down:
			menu_item_select_next(server);
			break;
		case XKB_KEY_Up:
			menu_item_select_previous(server);
			break;
		case XKB_KEY_Right:
			menu_submenu_enter(server);
			break;
		case XKB_KEY_Left:
			menu_submenu_leave(server);
			break;
		case XKB_KEY_Return:
			if (menu_call_selected_actions(server)) {
				menu_close_root(server);
				cursor_update_focus(server);
			}
			break;
		case XKB_KEY_Escape:
			menu_close_root(server);
			cursor_update_focus(server);
			break;
		default:
			continue;
		}
		break;
	}
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
	struct keysyms translated = { 0 };
	translated.nr_syms = xkb_state_key_get_syms(wlr_keyboard->xkb_state,
		keycode, &translated.syms);

	/*
	 * Get keysyms from the keyboard as if there was no modifier
	 * translations. For example, get Shift+1 rather than Shift+! (with US
	 * keyboard layout).
	 */
	struct keysyms raw = { 0 };
	xkb_layout_index_t layout_index =
		xkb_state_key_get_layout(wlr_keyboard->xkb_state, keycode);
	raw.nr_syms = xkb_keymap_key_get_syms_by_level(wlr_keyboard->keymap,
		keycode, layout_index, 0, &raw.syms);

	/*
	 * keyboard_key_notify() is called before keyboard_key_modifier(), so
	 * 'modifiers' refers to modifiers that were pressed before the key
	 * event in hand. Consequently, we use is_modifier_key() to find out if
	 * the key event being processed is a modifier.
	 *
	 * Sway solves this differently by saving the 'modifiers' state and
	 * checking if it has changed each time we get to the equivalent of this
	 * function. If it has changed, it concludes that the last key was a
	 * modifier and then deletes it from the buffer of pressed keycodes.
	 * For us the equivalent would look something like this:
	 *
	 * static uint32_t last_modifiers;
	 * bool last_key_was_a_modifier = last_modifiers != modifiers;
	 * last_modifiers = modifiers;
	 * if (last_key_was_a_modifier) {
	 *	key_state_remove_last_pressed_key(last_pressed_keycode);
	 * }
	 */

	bool is_modifier = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_keyboard);

	for (int i = 0; i < translated.nr_syms; i++) {
		is_modifier |= is_modifier_key(translated.syms[i]);
	}

	key_state_set_pressed(event->keycode,
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED);

	if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		/*
		 * Release events for keys that were not bound should
		 * always be forwarded to clients to avoid stuck keys.
		 */
		if (!key_state_corresponding_press_event_was_bound(
				event->keycode)) {
			return false;
		}

		/*
		 * If a user lets go of the modifier (e.g. alt) before
		 * the 'normal' key (e.g. tab) when window-cycling, we
		 * do not end the cycling until both keys have been
		 * released. If we end the window-cycling on release of
		 * the modifier only, some XWayland clients such as
		 * hexchat realise that tab is pressed (even though we
		 * did not forward the event) and because we absorb the
		 * equivalent release event it gets stuck on repeat.
		 */
		if (should_cancel_cycling_on_next_key_release) {
			end_cycling(server);
		}

		/*
		 * If a press event was handled by a compositor binding,
		 * then do not forward the corresponding release event
		 * to clients.
		 */
		key_state_bound_key_remove(event->keycode);
		return true;
	}

	/*
	 * Note: checks of event->state == WL_KEYBOARD_KEY_STATE_PRESSED
	 * below are redundant, but are left in place for now.
	 */
	bool handled = false;

	/* Catch C-A-F1 to C-A-F12 to change tty */
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < translated.nr_syms; i++) {
			unsigned int vt = translated.syms[i] - XKB_KEY_XF86Switch_VT_1 + 1;
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

	/*
	 * Ignore labwc keybindings if input is inhibited
	 * It's important to do this after key_state_set_pressed() to ensure
	 * _all_ key press/releases are registered
	 */
	if (seat->active_client_while_inhibited) {
		return false;
	}
	if (seat->server->session_lock) {
		return false;
	}

	if (server->input_mode == LAB_INPUT_STATE_MENU) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			handle_menu_keys(server, &translated);
		}
		handled = true;
		goto out;
	}

	if (server->osd_state.cycle_view) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			for (int i = 0; i < translated.nr_syms; i++) {
				if (translated.syms[i] == XKB_KEY_Escape) {
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
			if (!is_modifier) {
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

	/*
	 * Handle compositor keybinds
	 *
	 * When matching against keybinds, we process the input keys in the
	 * following order of precedence:
	 *   a. Keycodes (of physical keys) (not if keybind is layoutDependent)
	 *   b. Translated keysyms (taking into account modifiers, so if Shift+1
	 *      were pressed on a us keyboard, the keysym would be '!')
	 *   c. Raw keysyms (ignoring modifiers such as shift, so in the above
	 *      example the keysym would just be '1')
	 *
	 * The reasons for this approach are:
	 *   1. To make keybinds keyboard-layout agnostic (by checking keycodes
	 *      before keysyms). This means that in a multi-layout situation,
	 *      keybinds work regardless of which layout is active at the time
	 *      of the key-press.
	 *   2. To support keybinds relating to keysyms that are only available
	 *      in a particular layout, for example å, ä and ö.
	 *   3. To support keybinds that are only valid with a modifier, for
	 *      example the numpad keys with NumLock enabled: KP_x. These would
	 *      only be matched by the translated keysyms.
	 *   4. To support keybinds such as `S-1` (by checking raw keysyms).
	 *
	 * Reason 4 will also be satisfied by matching the keycodes. However,
	 * when a keybind is configured to be layoutDependent we still need
	 * the raw keysym fallback.
	 */

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* First try keycodes */
		handled |= handle_keybinding(server, modifiers, XKB_KEY_NoSymbol, keycode);
		if (handled) {
			wlr_log(WLR_DEBUG, "keycodes matched");
			goto out;
		}

		/* Then fall back to keysyms */
		for (int i = 0; i < translated.nr_syms; i++) {
			handled |= handle_keybinding(server, modifiers,
				translated.syms[i], keycode);
		}
		if (handled) {
			wlr_log(WLR_DEBUG, "translated keysyms matched");
			goto out;
		}

		/* And finally test for keysyms without modifier */
		for (int i = 0; i < raw.nr_syms; i++) {
			handled |= handle_keybinding(server, modifiers, raw.syms[i], keycode);
		}
		if (handled) {
			wlr_log(WLR_DEBUG, "raw keysyms matched");
		}
	}

out:
	if (handled) {
		key_state_store_pressed_key_as_bound(event->keycode);
	}

	return handled;
}

static int
handle_keybind_repeat(void *data)
{
	struct keyboard *keyboard = data;
	assert(keyboard->keybind_repeat);
	assert(keyboard->keybind_repeat_rate > 0);

	/* synthesize event */
	struct wlr_keyboard_key_event event = {
		.keycode = keyboard->keybind_repeat_keycode,
		.state = WL_KEYBOARD_KEY_STATE_PRESSED
	};

	handle_compositor_keybindings(keyboard, &event);
	int next_repeat_ms = 1000 / keyboard->keybind_repeat_rate;
	wl_event_source_timer_update(keyboard->keybind_repeat,
		next_repeat_ms);

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

static void
keyboard_key_notify(struct wl_listener *listener, void *data)
{
	/* This event is raised when a key is pressed or released. */
	struct keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct seat *seat = keyboard->base.seat;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *wlr_seat = seat->seat;
	idle_manager_notify_activity(seat->seat);

	/* any new press/release cancels current keybind repeat */
	keyboard_cancel_keybind_repeat(keyboard);

	bool handled = handle_compositor_keybindings(keyboard, event);
	if (handled) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			start_keybind_repeat(seat->server, keyboard, event);
		}
	} else {
		wlr_seat_set_keyboard(wlr_seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
			event->keycode, event->state);
	}
}

void
keyboard_set_numlock(struct wlr_keyboard *keyboard)
{
	xkb_mod_index_t num_idx =
		xkb_map_mod_get_index(keyboard->keymap, XKB_MOD_NAME_NUM);
	if (num_idx == XKB_MOD_INVALID) {
		wlr_log(WLR_INFO, "Failed to set Num Lock: not found in keymap");
		return;
	}

	xkb_mod_mask_t locked = keyboard->modifiers.locked;
	if (rc.kb_numlock_enable) {
		locked |= (xkb_mod_mask_t)1 << num_idx;
	} else {
		locked &= ~((xkb_mod_mask_t)1 << num_idx);
	}

	/*
	 * This updates the xkb-state + kb->modifiers and also triggers the
	 * keyboard->events.modifiers signal (the signal has no effect in
	 * current labwc usage since the keyboard is not part of a
	 * keyboard-group yet).
	 */
	wlr_keyboard_notify_modifiers(keyboard, keyboard->modifiers.depressed,
		keyboard->modifiers.latched, locked, keyboard->modifiers.group);
}

void
keyboard_update_layout(struct seat *seat, xkb_layout_index_t layout)
{
	assert(seat);

	struct input *input;
	struct keyboard *keyboard;
	struct wlr_keyboard *kb = NULL;

	/* We are not using wlr_seat_get_keyboard() here because it might be a virtual one */
	wl_list_for_each(input, &seat->inputs, link) {
		if (input->wlr_input_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}
		keyboard = (struct keyboard *)input;
		if (keyboard->is_virtual) {
			continue;
		}
		kb = keyboard->wlr_keyboard;
		break;
	}
	if (!kb) {
		wlr_log(WLR_INFO, "Restoring kb layout failed: no physical keyboard found");
		return;
	}
	if (kb->modifiers.group == layout) {
		return;
	}

	/* By updating a member of the keyboard group, all members of the group will get updated */
	wlr_log(WLR_DEBUG, "Updating group layout to %u", layout);
	wlr_keyboard_notify_modifiers(kb, kb->modifiers.depressed,
		kb->modifiers.latched, kb->modifiers.locked, layout);
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

	keybind_update_keycodes(seat->server);
}

void
keyboard_setup_handlers(struct keyboard *keyboard)
{
	struct wlr_keyboard *wlr_kb = keyboard->wlr_keyboard;

	keyboard->key.notify = keyboard_key_notify;
	wl_signal_add(&wlr_kb->events.key, &keyboard->key);
	keyboard->modifier.notify = keyboard_modifiers_notify;
	wl_signal_add(&wlr_kb->events.modifiers, &keyboard->modifier);
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
