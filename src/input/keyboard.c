// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "input/keyboard.h"
#include <assert.h>
#include <stdlib.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_seat.h>
#include "action.h"
#include "common/macros.h"
#include "config/keybind.h"
#include "config/rcxml.h"
#include "cycle.h"
#include "idle.h"
#include "input/ime.h"
#include "input/key-state.h"
#include "labwc.h"
#include "menu/menu.h"
#include "session-lock.h"
#include "view.h"
#include "workspaces.h"

#include <wlr/types/wlr_scene.h>
#include "common/buf.h"
#include "scaled-buffer/scaled-font-buffer.h"

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

uint32_t
keyboard_get_all_modifiers(struct seat *seat)
{
	/*
	 * As virtual keyboards like used by wayvnc are not part of the keyboard group,
	 * we need to additionally get the modifiers of the virtual keyboards in addition
	 * to the physical ones in the keyboard group. This ensures that mousebinds with
	 * keyboard modifiers are detected correctly when using for example a VNC client
	 * via wayvnc to control labwc. This function also gets called to decide when to
	 * hide the window switcher and workspace OSDs and to indicate if the user wants
	 * to snap a window to a region during a window move operation.
	 */
	struct input *input;
	uint32_t modifiers = wlr_keyboard_get_modifiers(&seat->keyboard_group->keyboard);
	wl_list_for_each(input, &seat->inputs, link) {
		if (input->wlr_input_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}
		struct keyboard *kb = wl_container_of(input, kb, base);
		if (kb->is_virtual) {
			modifiers |= wlr_keyboard_get_modifiers(kb->wlr_keyboard);
		}
	}
	return modifiers;
}

static struct wlr_seat_client *
seat_client_from_keyboard_resource(struct wl_resource *resource)
{
	return wl_resource_get_user_data(resource);
}

static void
broadcast_modifiers_to_unfocused_clients(struct wlr_seat *seat,
		const struct keyboard *keyboard,
		const struct wlr_keyboard_modifiers *modifiers)
{
	/* Prevent overwriting the group modifier by a virtual keyboard */
	if (keyboard->is_virtual) {
		return;
	}

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

static const char *
keycode_to_keyname(struct xkb_keymap *keymap, uint32_t keycode)
{
	const xkb_keysym_t *syms;
	int syms_len = xkb_keymap_key_get_syms_by_level(keymap, keycode + 8, 0, 0, &syms);
	if (!syms_len)
		return NULL;

	static char buf[256];
	if (!xkb_keysym_get_name(syms[0], buf, sizeof(buf)))
		return NULL;

	return buf;
}

static const char *
modifier_to_name(uint32_t modifier)
{
	switch (modifier) {
	case WLR_MODIFIER_SHIFT:
		return "S";
	case WLR_MODIFIER_CAPS:
		return "caps";
	case WLR_MODIFIER_CTRL:
		return "C";
	case WLR_MODIFIER_ALT:
		return "A";
	case WLR_MODIFIER_MOD2:
		return "mod2";
	case WLR_MODIFIER_MOD3:
		return "H";
	case WLR_MODIFIER_LOGO:
		return "W";
	case WLR_MODIFIER_MOD5:
		return "M";
	default:
		return "?";
	}
}

static void
update_key_indicator_callback(void *data)
{
	struct seat *seat = data;
	uint32_t all_modifiers = keyboard_get_all_modifiers(seat);
	float black[4] = {0, 0, 0, 1};
	float white[4] = {1, 1, 1, 1};

	extern struct lab_set pressed, bound, pressed_sent;

	static struct ctx {
		struct wlr_scene_tree *tree;
		struct scaled_font_buffer *sfb_pressed;
		struct scaled_font_buffer *sfb_bound;
		struct scaled_font_buffer *sfb_pressed_sent;
		struct scaled_font_buffer *sfb_modifiers;
		struct xkb_keymap *keymap;
	} ctx;
	if (!ctx.tree) {
		ctx.tree = wlr_scene_tree_create(&seat->server->scene->tree);
		wlr_scene_node_set_enabled(&ctx.tree->node, false);

		ctx.sfb_pressed = scaled_font_buffer_create(ctx.tree);
		wlr_scene_node_set_position(&ctx.sfb_pressed->scene_buffer->node, 0, 0);
		ctx.sfb_bound = scaled_font_buffer_create(ctx.tree);
		wlr_scene_node_set_position(&ctx.sfb_bound->scene_buffer->node, 0, 20);
		ctx.sfb_pressed_sent = scaled_font_buffer_create(ctx.tree);
		wlr_scene_node_set_position(&ctx.sfb_pressed_sent->scene_buffer->node, 0, 40);
		ctx.sfb_modifiers = scaled_font_buffer_create(ctx.tree);
		wlr_scene_node_set_position(&ctx.sfb_modifiers->scene_buffer->node, 0, 60);

		struct xkb_rule_names rules = { 0 };
		struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		ctx.keymap = xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}

	if (all_modifiers == (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT)) {
		for (int i = 0; i < pressed.size; i++) {
			if (pressed.values[i] == 44) { // Z
				wlr_log(WLR_ERROR, "enabling key indicator");
				wlr_scene_node_set_enabled(&ctx.tree->node, true);
				break;
			} else if (pressed.values[i] == 45) { // X
				wlr_log(WLR_ERROR, "disabling key indicator");
				wlr_scene_node_set_enabled(&ctx.tree->node, false);
				break;
			}
		}
	}
	if (!ctx.tree->node.enabled) {
		return;
	}

	struct buf buf = BUF_INIT;

	buf_reset(&buf);
	buf_add_fmt(&buf, "pressed=");
	for (int i = 0; i < pressed.size; i++) {
		const char *keyname = keycode_to_keyname(ctx.keymap, pressed.values[i]);
		buf_add_fmt(&buf, "%s (%d), ", keyname, pressed.values[i]);
	}
	scaled_font_buffer_update(ctx.sfb_pressed, buf.data, -1, &rc.font_osd, black, white);

	buf_reset(&buf);
	buf_add_fmt(&buf, "bound=");
	for (int i = 0; i < bound.size; i++) {
		const char *keyname = keycode_to_keyname(ctx.keymap, bound.values[i]);
		buf_add_fmt(&buf, "%s (%d), ", keyname, bound.values[i]);
	}
	scaled_font_buffer_update(ctx.sfb_bound, buf.data, -1, &rc.font_osd, black, white);

	buf_reset(&buf);
	buf_add_fmt(&buf, "pressed_sent=");
	for (int i = 0; i < pressed_sent.size; i++) {
		const char *keyname = keycode_to_keyname(ctx.keymap, pressed_sent.values[i]);
		buf_add_fmt(&buf, "%s (%d), ", keyname, pressed_sent.values[i]);
	}
	scaled_font_buffer_update(ctx.sfb_pressed_sent, buf.data, -1, &rc.font_osd, black, white);

	buf_reset(&buf);
	buf_add_fmt(&buf, "modifiers=");
	for (int i = 0; i <= 7; i++) {
		uint32_t mod = 1 << i;
		if (all_modifiers & mod) {
			buf_add_fmt(&buf, "%s, ", modifier_to_name(mod));
		}
	}
	buf_add_fmt(&buf, "(%d)", all_modifiers);
	scaled_font_buffer_update(ctx.sfb_modifiers, buf.data, -1, &rc.font_osd, black, white);

	buf_reset(&buf);
}

static void
update_key_indicator(struct seat *seat)
{
	wl_event_loop_add_idle(seat->server->wl_event_loop,
		update_key_indicator_callback, seat);
}

static void
handle_modifiers(struct wl_listener *listener, void *data)
{
	struct keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
	struct seat *seat = keyboard->base.seat;
	struct server *server = seat->server;
	struct wlr_keyboard *wlr_keyboard = keyboard->wlr_keyboard;

	update_key_indicator(seat);

	if (server->input_mode == LAB_INPUT_STATE_MOVE) {
		/* Any change to the modifier state re-enable region snap */
		seat->region_prevent_snap = false;
		/* Pressing/releasing modifier key may show/hide region overlay */
		overlay_update(seat);
	}

	bool cycling = server->input_mode == LAB_INPUT_STATE_CYCLE;

	if ((cycling || seat->workspace_osd_shown_by_modifier)
			&& !keyboard_get_all_modifiers(seat)) {
		if (cycling) {
			if (key_state_nr_bound_keys()) {
				should_cancel_cycling_on_next_key_release = true;
			} else {
				should_cancel_cycling_on_next_key_release = false;
				cycle_finish(server, /*switch_focus*/ true);
			}
		}
		if (seat->workspace_osd_shown_by_modifier) {
			workspaces_osd_hide(seat);
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
			keyboard, &wlr_keyboard->modifiers);
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
		if (view_inhibits_actions(server->active_view, &keybind->actions)) {
			continue;
		}
		if (sym == XKB_KEY_NoSymbol) {
			/* Use keycodes */
			if (keybind_contains_keycode(keybind, xkb_keycode)) {
				return keybind;
			}
		} else {
			/* Use syms */
			if (keybind_contains_keysym(keybind,
					xkb_keysym_to_lower(sym))) {
				return keybind;
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
	if (!is_virtual) {
		/* First try keycodes */
		struct keybind *keybind = match_keybinding_for_sym(server,
			keyinfo->modifiers, XKB_KEY_NoSymbol, keyinfo->xkb_keycode);
		if (keybind) {
			wlr_log(WLR_DEBUG, "keycode matched");
			return keybind;
		}
	}

	/* Then fall back to keysyms */
	for (int i = 0; i < keyinfo->translated.nr_syms; i++) {
		struct keybind *keybind =
			match_keybinding_for_sym(server, keyinfo->modifiers,
				keyinfo->translated.syms[i], keyinfo->xkb_keycode);
		if (keybind) {
			wlr_log(WLR_DEBUG, "translated keysym matched");
			return keybind;
		}
	}

	/* And finally test for keysyms without modifier */
	for (int i = 0; i < keyinfo->raw.nr_syms; i++) {
		struct keybind *keybind =
			match_keybinding_for_sym(server, keyinfo->modifiers,
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
	 * handle_key() is called before handle_modifiers(),
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

static enum lab_key_handled
handle_key_release(struct server *server, uint32_t evdev_keycode)
{
	/*
	 * Release events for keys that were not bound should always be
	 * forwarded to clients to avoid stuck keys.
	 */
	if (!key_state_corresponding_press_event_was_bound(evdev_keycode)) {
		return LAB_KEY_HANDLED_FALSE;
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
		should_cancel_cycling_on_next_key_release = false;
		cycle_finish(server, /*switch_focus*/ true);
	}

	/*
	 * If a press event was handled by a compositor binding, then do
	 * not forward the corresponding release event to clients.
	 */
	key_state_bound_key_remove(evdev_keycode);
	return LAB_KEY_HANDLED_TRUE;
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
		case XKB_KEY_KP_Enter:
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

/* Returns true if the keystroke is consumed */
static bool
handle_cycle_view_key(struct server *server, struct keyinfo *keyinfo)
{
	if (keyinfo->is_modifier) {
		return false;
	}

	/* cycle to next */
	for (int i = 0; i < keyinfo->translated.nr_syms; i++) {
		if (keyinfo->translated.syms[i] == XKB_KEY_Escape) {
			/* Esc deactivates window switcher */
			cycle_finish(server, /*switch_focus*/ false);
			return true;
		}
		if (keyinfo->translated.syms[i] == XKB_KEY_Up
				|| keyinfo->translated.syms[i] == XKB_KEY_Left) {
			/* Up/Left cycles the window backward */
			cycle_step(server, LAB_CYCLE_DIR_BACKWARD);
			return true;
		}
		if (keyinfo->translated.syms[i] == XKB_KEY_Down
				|| keyinfo->translated.syms[i] == XKB_KEY_Right) {
			/* Down/Right cycles the window forward */
			cycle_step(server, LAB_CYCLE_DIR_FORWARD);
			return true;
		}
	}
	return false;
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
		event->state == WL_KEYBOARD_KEY_STATE_PRESSED);

	if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		if (cur_keybind && cur_keybind->on_release) {
			key_state_bound_key_remove(event->keycode);
			if (locked && !cur_keybind->allow_when_locked) {
				cur_keybind = NULL;
				return LAB_KEY_HANDLED_TRUE;
			}
			actions_run(NULL, server, &cur_keybind->actions, NULL);
			return LAB_KEY_HANDLED_TRUE;
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
			return LAB_KEY_HANDLED_TRUE;
		} else if (server->input_mode == LAB_INPUT_STATE_CYCLE) {
			if (handle_cycle_view_key(server, &keyinfo)) {
				key_state_store_pressed_key_as_bound(event->keycode);
				return LAB_KEY_HANDLED_TRUE;
			}
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
		return LAB_KEY_HANDLED_TRUE;
	}

	return LAB_KEY_HANDLED_FALSE;
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

void
keyboard_cancel_all_keybind_repeats(struct seat *seat)
{
	struct input *input;
	struct keyboard *kb;
	wl_list_for_each(input, &seat->inputs, link) {
		if (input->wlr_input_device->type == WLR_INPUT_DEVICE_KEYBOARD) {
			kb = wl_container_of(input, kb, base);
			keyboard_cancel_keybind_repeat(kb);
		}
	}
}

static void
handle_key(struct wl_listener *listener, void *data)
{
	/* This event is raised when a key is pressed or released. */
	struct keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct seat *seat = keyboard->base.seat;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *wlr_seat = seat->seat;

	update_key_indicator(seat);

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
	if (rc.kb_numlock_enable == LAB_STATE_UNSPECIFIED) {
		return;
	}

	xkb_mod_index_t num_idx =
		xkb_map_mod_get_index(keyboard->keymap, XKB_MOD_NAME_NUM);
	if (num_idx == XKB_MOD_INVALID) {
		wlr_log(WLR_INFO, "Failed to set Num Lock: not found in keymap");
		return;
	}

	xkb_mod_mask_t locked = keyboard->modifiers.locked;
	if (rc.kb_numlock_enable == LAB_STATE_ENABLED) {
		locked |= (xkb_mod_mask_t)1 << num_idx;
	} else if (rc.kb_numlock_enable == LAB_STATE_DISABLED) {
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
	CONNECT_SIGNAL(keyboard->wlr_keyboard, keyboard, key);
	CONNECT_SIGNAL(keyboard->wlr_keyboard, keyboard, modifiers);
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
