/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_KEYBOARD_H
#define LABWC_KEYBOARD_H

#include <stdbool.h>

struct seat;
struct keyboard;
struct wlr_keyboard;

void keyboard_init(struct seat *seat);
void keyboard_finish(struct seat *seat);

void keyboard_setup_handlers(struct keyboard *keyboard);
void keyboard_set_numlock(struct wlr_keyboard *keyboard);
void keyboard_cancel_keybind_repeat(struct keyboard *keyboard);
bool keyboard_any_modifiers_pressed(struct wlr_keyboard *keyboard);

#endif /* LABWC_KEYBOARD_H */
