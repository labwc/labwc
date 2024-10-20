// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include "common/set.h"
#include "input/key-state.h"

static struct lab_set pressed, pressed_mods, bound, pressed_sent;

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
key_state_set_pressed(uint32_t keycode, bool is_pressed, bool is_modifier)
{
	if (is_pressed) {
		lab_set_add(&pressed, keycode);
		if (is_modifier) {
			lab_set_add(&pressed_mods, keycode);
		}
	} else {
		lab_set_remove(&pressed, keycode);
		lab_set_remove(&pressed_mods, keycode);
	}
}

void
key_state_store_pressed_key_as_bound(uint32_t keycode)
{
	lab_set_add(&bound, keycode);
	/*
	 * Also store any pressed modifiers as bound. This prevents
	 * applications from seeing and handling the release event for
	 * a modifier key that was part of a keybinding (e.g. Firefox
	 * displays its menu bar for a lone Alt press + release).
	 */
	for (int i = 0; i < pressed_mods.size; ++i) {
		lab_set_add(&bound, pressed_mods.values[i]);
	}
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

int
key_state_nr_pressed_keys(void)
{
	return pressed.size;
}
