// SPDX-License-Identifier: GPL-2.0-only
#include "input/key-state.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_scene.h>
#include <xkbcommon/xkbcommon.h>
#include "config/rcxml.h"
#include "common/buf.h"
#include "common/set.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "scaled-buffer/scaled-font-buffer.h"

static struct lab_set pressed, bound, pressed_sent;

static bool show_debug_indicator;
static struct indicator_state {
	struct wlr_scene_tree *tree;
	struct scaled_font_buffer *sfb_pressed;
	struct scaled_font_buffer *sfb_bound;
	struct scaled_font_buffer *sfb_pressed_sent;
	struct scaled_font_buffer *sfb_modifiers;
	struct xkb_keymap *keymap;
} indicator_state;

static const char *
keycode_to_keyname(struct xkb_keymap *keymap, uint32_t keycode)
{
	const xkb_keysym_t *syms;
	int syms_len = xkb_keymap_key_get_syms_by_level(keymap, keycode + 8, 0, 0, &syms);
	if (!syms_len) {
		return NULL;
	}

	static char buf[256];
	if (!xkb_keysym_get_name(syms[0], buf, sizeof(buf))) {
		return NULL;
	}

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
		return "numlock";
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
init_indicator(struct indicator_state *state)
{
	state->tree = wlr_scene_tree_create(&server.scene->tree);
	wlr_scene_node_set_enabled(&state->tree->node, false);

	state->sfb_pressed = scaled_font_buffer_create(state->tree);
	wlr_scene_node_set_position(&state->sfb_pressed->scene_buffer->node, 0, 0);
	state->sfb_bound = scaled_font_buffer_create(state->tree);
	wlr_scene_node_set_position(&state->sfb_bound->scene_buffer->node, 0, 20);
	state->sfb_pressed_sent = scaled_font_buffer_create(state->tree);
	wlr_scene_node_set_position(&state->sfb_pressed_sent->scene_buffer->node, 0, 40);
	state->sfb_modifiers = scaled_font_buffer_create(state->tree);
	wlr_scene_node_set_position(&state->sfb_modifiers->scene_buffer->node, 0, 60);

	struct xkb_rule_names rules = { 0 };
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	state->keymap = xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

static void
update_key_indicator_callback(void *data)
{
	struct seat *seat = data;
	uint32_t all_modifiers = keyboard_get_all_modifiers(seat);
	float black[4] = {0, 0, 0, 1};
	float white[4] = {1, 1, 1, 1};

	if (!indicator_state.tree) {
		init_indicator(&indicator_state);
	}

	if (show_debug_indicator) {
		wlr_scene_node_set_enabled(&indicator_state.tree->node, true);
	} else {
		wlr_scene_node_set_enabled(&indicator_state.tree->node, false);
		return;
	}

	struct buf buf = BUF_INIT;

	buf_add(&buf, "pressed=");
	for (int i = 0; i < pressed.size; i++) {
		const char *keyname = keycode_to_keyname(indicator_state.keymap,
					pressed.values[i]);
		buf_add_fmt(&buf, "%s (%d), ", keyname, pressed.values[i]);
	}
	scaled_font_buffer_update(indicator_state.sfb_pressed, buf.data,
		-1, &rc.font_osd, black, white, false);

	buf_clear(&buf);
	buf_add(&buf, "bound=");
	for (int i = 0; i < bound.size; i++) {
		const char *keyname = keycode_to_keyname(indicator_state.keymap,
					bound.values[i]);
		buf_add_fmt(&buf, "%s (%d), ", keyname, bound.values[i]);
	}
	scaled_font_buffer_update(indicator_state.sfb_bound, buf.data,
		-1, &rc.font_osd, black, white, false);

	buf_clear(&buf);
	buf_add(&buf, "pressed_sent=");
	for (int i = 0; i < pressed_sent.size; i++) {
		const char *keyname = keycode_to_keyname(indicator_state.keymap,
					pressed_sent.values[i]);
		buf_add_fmt(&buf, "%s (%d), ", keyname, pressed_sent.values[i]);
	}
	scaled_font_buffer_update(indicator_state.sfb_pressed_sent, buf.data, -1,
		&rc.font_osd, black, white, false);

	buf_clear(&buf);
	buf_add(&buf, "modifiers=");
	for (int i = 0; i <= 7; i++) {
		uint32_t mod = 1 << i;
		if (all_modifiers & mod) {
			buf_add_fmt(&buf, "%s, ", modifier_to_name(mod));
		}
	}
	buf_add_fmt(&buf, "(%d)", all_modifiers);
	scaled_font_buffer_update(indicator_state.sfb_modifiers, buf.data, -1,
		&rc.font_osd, black, white, false);

	buf_reset(&buf);
}

void
key_state_indicator_update(struct seat *seat)
{
	if (!show_debug_indicator) {
		return;
	}
	wl_event_loop_add_idle(server.wl_event_loop,
		update_key_indicator_callback, seat);
}

void
key_state_indicator_toggle(void)
{
	show_debug_indicator = !show_debug_indicator;
}

static void
report(struct lab_set *key_set, const char *msg)
{
	static char *should_print;
	static bool has_run;

	if (!has_run) {
		should_print = getenv("LABWC_DEBUG_KEY_STATE");
		has_run = true;
	}
	if (!should_print) {
		return;
	}
	printf("%s", msg);
	for (int i = 0; i < key_set->size; ++i) {
		printf("%d,", key_set->values[i]);
	}
	printf("\n");
}

uint32_t *
key_state_pressed_sent_keycodes(void)
{
	report(&pressed, "before - pressed:");
	report(&bound, "before - bound:");

	/* pressed_sent = pressed - bound */
	pressed_sent = pressed;
	for (int i = 0; i < bound.size; ++i) {
		lab_set_remove(&pressed_sent, bound.values[i]);
	}

	report(&pressed_sent, "after - pressed_sent:");

	return pressed_sent.values;
}

int
key_state_nr_pressed_sent_keycodes(void)
{
	return pressed_sent.size;
}

void
key_state_set_pressed(uint32_t keycode, bool is_pressed)
{
	if (is_pressed) {
		lab_set_add(&pressed, keycode);
	} else {
		lab_set_remove(&pressed, keycode);
	}
}

void
key_state_store_pressed_key_as_bound(uint32_t keycode)
{
	lab_set_add(&bound, keycode);
}

bool
key_state_corresponding_press_event_was_bound(uint32_t keycode)
{
	return lab_set_contains(&bound, keycode);
}

void
key_state_bound_key_remove(uint32_t keycode)
{
	lab_set_remove(&bound, keycode);
}

int
key_state_nr_bound_keys(void)
{
	return bound.size;
}
