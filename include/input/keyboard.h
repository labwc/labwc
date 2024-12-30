/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_KEYBOARD_H
#define LABWC_KEYBOARD_H

#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

struct seat;
struct keyboard;
struct wlr_keyboard;

void keyboard_reset_current_keybind(void);
void keyboard_configure(struct seat *seat, struct wlr_keyboard *kb,
	bool is_virtual);

void keyboard_group_init(struct seat *seat);
void keyboard_group_finish(struct seat *seat);

void keyboard_setup_handlers(struct keyboard *keyboard);
void keyboard_set_numlock(struct wlr_keyboard *keyboard);
void keyboard_update_layout(struct seat *seat, xkb_layout_index_t layout);
void keyboard_cancel_keybind_repeat(struct keyboard *keyboard);
void keyboard_cancel_all_keybind_repeats(struct seat *seat);

bool keyboard_any_modifiers_pressed(struct wlr_keyboard *keyboard);

#endif /* LABWC_KEYBOARD_H */
