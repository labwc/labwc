/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_KEY_STATE_H
#define __LABWC_KEY_STATE_H

void key_state_set_pressed(uint32_t keycode, bool ispressed);
void key_state_store_pressed_keys_as_bound(void);
bool key_state_corresponding_press_event_was_bound(uint32_t keycode);
void key_state_bound_key_remove(uint32_t keycode);
int key_state_nr_keys(void);

#endif /* __LABWC_KEY_STATE_H */
