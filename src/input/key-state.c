// SPDX-License-Identifier: GPL-2.0-only
#include "input/key-state.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "common/set.h"

static struct lab_set pressed, bound, pressed_sent;

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

int
key_state_nr_pressed_keys(void)
{
	return pressed.size;
}
