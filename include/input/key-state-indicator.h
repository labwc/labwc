/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_KEY_STATE_INDICATOR_H
#define LABWC_KEY_STATE_INDICATOR_H

struct seat;

/*
 * All keycodes in these functions are (Linux) libinput evdev scancodes which is
 * what 'wlr_keyboard' uses (e.g. 'seat->keyboard_group->keyboard->keycodes').
 * Note: These keycodes are different to XKB scancodes by a value of 8.
 */

void key_state_indicator_update(struct seat *seat);
void key_state_indicator_toggle(void);

#endif /* LABWC_KEY_STATE_INDICATOR_H */
