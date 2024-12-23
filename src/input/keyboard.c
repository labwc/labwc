// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include "action.h"
#include "idle.h"
#include "input/ime.h"
#include "input/keyboard.h"
#include "input/key-state.h"
#include "labwc.h"
#include "menu/menu.h"
#include "osd.h"
#include "regions.h"
#include "view.h"
#include "workspaces.h"

enum lab_key_handled {
	LAB_KEY_HANDLED_FALSE = 0,
	LAB_KEY_HANDLED_TRUE = 1,
	LAB_KEY_HANDLED_TRUE_AND_VT_CHANGED,
};

struct keysyms {
	const xkb_keysym_t *syms;
	int nr_syms;
};

struct keyinfo {
	xkb_keycode_t xkb_keycode;
	struct keysyms translated;
	struct keysyms raw;
	uint32_t modifiers;
	bool is_modifier;
};

static bool should_cancel_cycling_on_next_key_release;

static struct keybind *cur_keybind;

/* Called on --reconfigure to prevent segfault when handling release keybinds */
void
keyboard_reset_current_keybind(void)
{
	cur_keybind = NULL;
}

static void
change_vt(struct server *server, unsigned int vt)
{
	wlr_session_change_vt(server->session, vt);
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
	osd_preview_restore(server);
	if (server->osd_state.cycle_view) {
		desktop_focus_view(server->osd_state.cycle_view,
			/*raise*/ true);
	}

	/* osd_finish() additionally resets cycle_view to NULL */
	osd_finish(server);
	should_cancel_cycling_on_next_key_release = false;
}

static struct wlr_seat_client *
seat_client_from_keyboard_resource(struct wl_resource *resource)
{
	return wl_resource_get_user_data(resource);
}

static void
broadcast_modifiers_to_unfocused_clients(struct wlr_seat *seat,
		const struct wlr_keyboard_modifiers *modifiers)
{
	struct wlr_seat_client *client;
	wl_list_for_each(client, &seat->clients, link) {
		if (client == seat->keyboard_state.focused_client) {
			/*
			 * We've already notified the focused client by calling
			 * wlr_seat_keyboard_notify_modifiers()
			 */
			continue;
		}
		uint32_t serial = wlr_seat_client_next_serial(client);
		struct wl_resource *resource;
		wl_resource_for_each(resource, &client->keyboards) {
			if (!seat_client_from_keyboard_resource(resource)) {
				continue;
			}
			if (!modifiers) {
				wl_keyboard_send_modifiers(resource, serial, 0,
					0, 0, 0);
			} else {
				wl_keyboard_send_modifiers(resource, serial,
					modifiers->depressed, modifiers->latched,
					modifiers->locked, modifiers->group);
			}
		}
	}
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
		/* Pressing/releasing modifier key may show/hide region overlay */
		overlay_update(seat);
	}

	if (server->osd_state.cycle_view
			|| seat->workspace_osd_shown_by_modifier) {
		if (!keyboard_any_modifiers_pressed(wlr_keyboard)) {
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
		}
	}

	if (!input_method_keyboard_grab_forward_modifiers(keyboard)) {
		/* Send modifiers to focused client */
		wlr_seat_keyboard_notify_modifiers(seat->seat,
			&wlr_keyboard->modifiers);

		/*
		 * Also broadcast them to non-keyboard-focused clients.
		 *
		 * The Wayland protocol does not specify that modifiers are
		 * broadcast, so this is not something clients can rely on in
		 * other compositors.
		 *
		 * Sway used to broadcast modifiers but stopped doing so to
		 * avoid waking up all clients when the modifiers change.
		 *
		 * By testing with foot and Ctrl+scroll to change font size, it
		 * appears that Mutter does not pass modifiers to unfocused
		 * clients, whereas KWin and Weston pass modifiers to clients
		 * with pointer-focus.
		 *
		 * This could be made configurable if there are unintended
		 * consequences. If so, modifiers ought to still be passed to
		 * clients with pointer-focus (see issue #2271)
		 */
		broadcast_modifiers_to_unfocused_clients(seat->seat,
			&wlr_keyboard->modifiers);
	}
}

static struct keybind *
match_keybinding_for_sym(struct server *server, uint32_t modifiers,
		xkb_keysym_t sym, xkb_keycode_t xkb_keycode)
{
	struct keybind *keybind;
	wl_list_for_each(keybind, &rc.keybinds, link) {
		if (modifiers ^ keybind->modifiers) {
			continue;
		}
		if (server->seat.nr_inhibited_keybind_views
				&& server->active_view
				&& server->active_view->inhibits_keybinds
				&& !actions_contain_toggle_keybinds(&keybind->actions)) {
			continue;
		}
		if (sym == XKB_KEY_NoSymbol) {
			/* Use keycodes */
			for (size_t i = 0; i < keybind->keycodes_len; i++) {
				if (keybind->keycodes[i] == xkb_keycode) {
					return keybind;
				}
			}
		} else {
			/* Use syms */
			for (size_t i = 0; i < keybind->keysyms_len; i++) {
				if (xkb_keysym_to_lower(sym) == keybind->keysyms[i]) {
					return keybind;
				}
			}
		}
	}
	return NULL;
}

/*
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
static struct keybind *
match_keybinding(struct server *server, struct keyinfo *keyinfo,
		bool is_virtual)
{
	if (is_virtual) {
		goto process_syms;
	}

	/* First try keycodes */
	struct keybind *keybind = match_keybinding_for_sym(server,
		keyinfo->modifiers, XKB_KEY_NoSymbol, keyinfo->xkb_keycode);
	if (keybind) {
		wlr_log(WLR_DEBUG, "keycode matched");
		return keybind;
	}

process_syms:
	/* Then fall back to keysyms */
	for (int i = 0; i < keyinfo->translated.nr_syms; i++) {
		keybind = match_keybinding_for_sym(server, keyinfo->modifiers,
			keyinfo->translated.syms[i], keyinfo->xkb_keycode);
		if (keybind) {
			wlr_log(WLR_DEBUG, "translated keysym matched");
			return keybind;
		}
	}

	/* And finally test for keysyms without modifier */
	for (int i = 0; i < keyinfo->raw.nr_syms; i++) {
		keybind = match_keybinding_for_sym(server, keyinfo->modifiers,
			keyinfo->raw.syms[i], keyinfo->xkb_keycode);
		if (keybind) {
			wlr_log(WLR_DEBUG, "raw keysym matched");
			return keybind;
		}
	}

	return NULL;
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

static bool
is_modifier(struct wlr_keyboard *wlr_keyboard, uint32_t evdev_keycode)
{
	const xkb_keysym_t *syms = NULL;
	int nr_syms = xkb_state_key_get_syms(wlr_keyboard->xkb_state,
		evdev_keycode + 8, &syms);
	for (int i = 0; i < nr_syms; i++) {
		if (is_modifier_key(syms[i])) {
			return true;
		}
	}
	return false;
}

static struct keyinfo
get_keyinfo(struct wlr_keyboard *wlr_keyboard, uint32_t evdev_keycode)
{
	struct keyinfo keyinfo = {0};

	/* Translate evdev/libinput keycode -> xkb */
	keyinfo.xkb_keycode = evdev_keycode + 8;

	/* Get a list of keysyms based on the keymap for this keyboard */
	keyinfo.translated.nr_syms = xkb_state_key_get_syms(
		wlr_keyboard->xkb_state, keyinfo.xkb_keycode,
		&keyinfo.translated.syms);

	/*
	 * Get keysyms from the keyboard as if there was no modifier
	 * translations. For example, get Shift+1 rather than Shift+!
	 * (with US keyboard layout).
	 */
	xkb_layout_index_t layout_index = xkb_state_key_get_layout(
		wlr_keyboard->xkb_state, keyinfo.xkb_keycode);
	keyinfo.raw.nr_syms = xkb_keymap_key_get_syms_by_level(
		wlr_keyboard->keymap, keyinfo.xkb_keycode, layout_index, 0,
		&keyinfo.raw.syms);

	/*
	 * keyboard_key_notify() is called before keyboard_key_modifier(),
	 * so 'modifiers' refers to modifiers that were pressed before the
	 * key event in hand. Consequently, we use is_modifier_key() to
	 * find out if the key event being processed is a modifier.
	 *
	 * Sway solves this differently by saving the 'modifiers' state
	 * and checking if it has changed each time we get to the equivalent
	 * of this function. If it has changed, it concludes that the last
	 * key was a modifier and then deletes it from the buffer of pressed
	 * keycodes. For us the equivalent would look something like this:
	 *
	 * static uint32_t last_modifiers;
	 * bool last_key_was_a_modifier = last_modifiers != modifiers;
	 * last_modifiers = modifiers;
	 * if (last_key_was_a_modifier) {
	 *   key_state_remove_last_pressed_key(last_pressed_keycode);
	 * }
	 */
	keyinfo.modifiers = wlr_keyboard_get_modifiers(wlr_keyboard);
	keyinfo.is_modifier = false;
	for (int i = 0; i < keyinfo.translated.nr_syms; i++) {
		keyinfo.is_modifier |=
			is_modifier_key(keyinfo.translated.syms[i]);
	}

	return keyinfo;
}

static bool
handle_key_release(struct server *server, uint32_t evdev_keycode)
{
	/*
	 * Release events for keys that were not bound should always be
	 * forwarded to clients to avoid stuck keys.
	 */
	if (!key_state_corresponding_press_event_was_bound(evdev_keycode)) {
		return false;
	}

	/*
	 * If a user lets go of the modifier (e.g. alt) before the 'normal'
	 * key (e.g. tab) when window-cycling, we do not end the cycling
	 * until both keys have been released. If we end the window-cycling
	 * on release of the modifier only, some XWayland clients such as
	 * hexchat realise that tab is pressed (even though we did not
	 * forward the event) and because we absorb the equivalent release
	 * event it gets stuck on repeat.
	 */
	if (should_cancel_cycling_on_next_key_release) {
		end_cycling(server);
	}

	/*
	 * If a press event was handled by a compositor binding, then do
	 * not forward the corresponding release event to clients.
	 */
	key_state_bound_key_remove(evdev_keycode);
	return true;
}

static bool
handle_change_vt_key(struct server *server, struct keyboard *keyboard,
		struct keysyms *translated)
{
	for (int i = 0; i < translated->nr_syms; i++) {
		unsigned int vt =
			translated->syms[i] - XKB_KEY_XF86Switch_VT_1 + 1;
		if (vt >= 1 && vt <= 12) {
			keyboard_cancel_keybind_repeat(keyboard);
			change_vt(server, vt);
			return true;
		}
	}
	return false;
}

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
			menu_call_selected_actions(server);
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

static void
toggle_direction(enum lab_cycle_dir *direction)
{
	if (*direction == LAB_CYCLE_DIR_FORWARD) {
		*direction = LAB_CYCLE_DIR_BACKWARD;
	} else if (*direction == LAB_CYCLE_DIR_BACKWARD) {
		*direction = LAB_CYCLE_DIR_FORWARD;
	}
}

static void
handle_cycle_view_key(struct server *server, struct keyinfo *keyinfo)
{
	for (int i = 0; i < keyinfo->translated.nr_syms; i++) {
		if (keyinfo->translated.syms[i] == XKB_KEY_Escape) {
			/* cancel view-cycle */
			osd_preview_restore(server);
			osd_finish(server);
			return;
		}
	}

	/* cycle to next */
	if (!keyinfo->is_modifier) {
		enum lab_cycle_dir direction = server->osd_state.initial_direction;
		for (int i = 0; i < keyinfo->translated.nr_syms; i++) {
			if (keyinfo->translated.syms[i] == XKB_KEY_Up
					|| keyinfo->translated.syms[i] == XKB_KEY_Left) {
				direction = LAB_CYCLE_DIR_BACKWARD;
				goto miss_shift_toggle;
			}
			if (keyinfo->translated.syms[i] == XKB_KEY_Down
					|| keyinfo->translated.syms[i] == XKB_KEY_Right) {
				direction = LAB_CYCLE_DIR_FORWARD;
				goto miss_shift_toggle;
			}
		}

		bool shift_is_pressed = keyinfo->modifiers & WLR_MODIFIER_SHIFT;
		if (shift_is_pressed != server->osd_state.initial_keybind_contained_shift) {
			/*
			 * Shift reverses the direction - unless shift was part of the
			 * original keybind in which case we do the opposite.
			 * For example with S-A-Tab bound to PreviousWindow, shift with
			 * subsequent key presses should carry on cycling backwards.
			 */
			toggle_direction(&direction);
		}

	/* Only one direction modifier is allowed, either arrow keys OR shift */
miss_shift_toggle:

		server->osd_state.cycle_view = desktop_cycle_view(server,
			server->osd_state.cycle_view, direction);
		osd_update(server);
	}
}

static enum lab_key_handled
handle_compositor_keybindings(struct keyboard *keyboard,
		struct wlr_keyboard_key_event *event)
{
	struct seat *seat = keyboard->base.seat;
	struct server *server = seat->server;
	struct wlr_keyboard *wlr_keyboard = keyboard->wlr_keyboard;
	struct keyinfo keyinfo = get_keyinfo(wlr_keyboard, event->keycode);
	bool locked = seat->server->session_lock_manager->locked;

	key_state_set_pressed(event->keycode,
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED,
		keyinfo.is_modifier);

	if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		if (cur_keybind && cur_keybind->on_release) {
			key_state_bound_key_remove(event->keycode);
			if (locked && !cur_keybind->allow_when_locked) {
				cur_keybind = NULL;
				return true;
			}
			actions_run(NULL, server, &cur_keybind->actions, NULL);
			return true;
		} else {
			return handle_key_release(server, event->keycode);
		}
	}

	/* Catch C-A-F1 to C-A-F12 to change tty */
	if (handle_change_vt_key(server, keyboard, &keyinfo.translated)) {
		key_state_store_pressed_key_as_bound(event->keycode);
		return LAB_KEY_HANDLED_TRUE_AND_VT_CHANGED;
	}

	/*
	 * Ignore labwc keybindings if the session is locked.
	 * It's important to do this after key_state_set_pressed() to ensure
	 * _all_ key press/releases are registered
	 */
	if (!locked) {
		if (server->input_mode == LAB_INPUT_STATE_MENU) {
			key_state_store_pressed_key_as_bound(event->keycode);
			handle_menu_keys(server, &keyinfo.translated);
			return true;
		}

		if (server->osd_state.cycle_view) {
			key_state_store_pressed_key_as_bound(event->keycode);
			handle_cycle_view_key(server, &keyinfo);
			return true;
		}
	}

	/*
	 * Handle compositor keybinds
	 */
	cur_keybind = match_keybinding(server, &keyinfo, keyboard->is_virtual);
	if (cur_keybind && (!locked || cur_keybind->allow_when_locked)) {
		/*
		 * Update key-state before action_run() because the action
		 * might lead to seat_focus() in which case we pass the
		 * 'pressed-sent' keys to the new surface.
		 */
		key_state_store_pressed_key_as_bound(event->keycode);
		if (!cur_keybind->on_release) {
			actions_run(NULL, server, &cur_keybind->actions, NULL);
		}
		return true;
	}

	return false;
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

	enum lab_key_handled handled =
		handle_compositor_keybindings(keyboard, event);

	if (handled == LAB_KEY_HANDLED_TRUE_AND_VT_CHANGED) {
		return;
	}

	if (handled) {
		/*
		 * We do not start the repeat-timer on pressed modifiers (like
		 * Super_L) because it is only for our own internal use with
		 * keybinds and it messes up modifier-onRelease-keybinds.
		 */
		if (!is_modifier(keyboard->wlr_keyboard, event->keycode)
				&& event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			start_keybind_repeat(seat->server, keyboard, event);
		}
	} else if (!input_method_keyboard_grab_forward_key(keyboard, event)) {
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

static void
reset_window_keyboard_layout_groups(struct server *server)
{
	if (!rc.kb_layout_per_window) {
		return;
	}

	/*
	 * Technically it would be possible to reconcile previous group indices
	 * to new group ones if particular layouts exist in both old and new,
	 * but let's keep it simple for now and just reset them all.
	 */
	struct view *view;
	for_each_view(view, &server->views, LAB_VIEW_CRITERIA_NONE) {
		view->keyboard_layout = 0;
	}

	struct view *active_view = server->active_view;
	if (!active_view) {
		return;
	}
	keyboard_update_layout(&server->seat, active_view->keyboard_layout);
}

/*
 * Set layout based on environment variables XKB_DEFAULT_LAYOUT,
 * XKB_DEFAULT_OPTIONS, and friends.
 */
static void
set_layout(struct server *server, struct wlr_keyboard *kb)
{
	static bool fallback_mode;

	struct xkb_rule_names rules = { 0 };
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, &rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	/*
	 * With XKB_DEFAULT_LAYOUT set to empty odd things happen with
	 * xkb_map_new_from_names() resulting in the keyboard not working, so
	 * we protect against that.
	 */
	const char *layout = getenv("XKB_DEFAULT_LAYOUT");
	bool layout_empty = layout && !*layout;
	if (keymap && !layout_empty) {
		if (!wlr_keyboard_keymaps_match(kb->keymap, keymap)) {
			wlr_keyboard_set_keymap(kb, keymap);
			reset_window_keyboard_layout_groups(server);
		}
		xkb_keymap_unref(keymap);
	} else {
		wlr_log(WLR_ERROR, "failed to create xkb keymap for layout '%s'",
			layout);
		if (!fallback_mode) {
			wlr_log(WLR_ERROR, "entering fallback mode with layout 'us'");
			fallback_mode = true;
			setenv("XKB_DEFAULT_LAYOUT", "us", 1);
			set_layout(server, kb);
		}
	}
	xkb_context_unref(context);
}

void
keyboard_configure(struct seat *seat, struct wlr_keyboard *kb, bool is_virtual)
{
	if (!is_virtual) {
		set_layout(seat->server, kb);
	}
	wlr_keyboard_set_repeat_info(kb, rc.repeat_rate, rc.repeat_delay);
	keybind_update_keycodes(seat->server);
}

void
keyboard_group_init(struct seat *seat)
{
	if (seat->keyboard_group) {
		return;
	}
	seat->keyboard_group = wlr_keyboard_group_create();
	keyboard_configure(seat, &seat->keyboard_group->keyboard,
		/* is_virtual */ false);
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
keyboard_group_finish(struct seat *seat)
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
