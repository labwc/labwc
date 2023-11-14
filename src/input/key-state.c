// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include "input/key-state.h"

#define MAX_PRESSED_KEYS (16)

struct key_array {
	uint32_t keys[MAX_PRESSED_KEYS];
	int nr_keys;
};

static struct key_array pressed, pressed_mods, bound, pressed_sent;

static void
report(struct key_array *array, const char *msg)
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
	for (int i = 0; i < array->nr_keys; ++i) {
		printf("%d,", array->keys[i]);
	}
	printf("\n");
}

static bool
key_present(struct key_array *array, uint32_t keycode)
{
	for (int i = 0; i < array->nr_keys; ++i) {
		if (array->keys[i] == keycode) {
			return true;
		}
	}
	return false;
}

static void
remove_key(struct key_array *array, uint32_t keycode)
{
	bool shifting = false;

	for (int i = 0; i < MAX_PRESSED_KEYS; ++i) {
		if (array->keys[i] == keycode) {
			--array->nr_keys;
			shifting = true;
		}
		if (shifting) {
			array->keys[i] = i < MAX_PRESSED_KEYS - 1
				? array->keys[i + 1] : 0;
		}
	}
}

static void
add_key(struct key_array *array, uint32_t keycode)
{
	if (!key_present(array, keycode) && array->nr_keys < MAX_PRESSED_KEYS) {
		array->keys[array->nr_keys++] = keycode;
	}
}

uint32_t *
key_state_pressed_sent_keycodes(void)
{
	report(&pressed, "before - pressed:");
	report(&bound, "before - bound:");

	/* pressed_sent = pressed - bound */
	memcpy(pressed_sent.keys, pressed.keys,
		MAX_PRESSED_KEYS * sizeof(uint32_t));
	pressed_sent.nr_keys = pressed.nr_keys;
	for (int i = 0; i < bound.nr_keys; ++i) {
		remove_key(&pressed_sent, bound.keys[i]);
	}

	report(&pressed_sent, "after - pressed_sent:");

	return pressed_sent.keys;
}

int
key_state_nr_pressed_sent_keycodes(void)
{
	return pressed_sent.nr_keys;
}

void
key_state_set_pressed(uint32_t keycode, bool is_pressed, bool is_modifier)
{
	if (is_pressed) {
		add_key(&pressed, keycode);
		if (is_modifier) {
			add_key(&pressed_mods, keycode);
		}
	} else {
		remove_key(&pressed, keycode);
		remove_key(&pressed_mods, keycode);
	}
}

void
key_state_store_pressed_key_as_bound(uint32_t keycode)
{
	add_key(&bound, keycode);
	/*
	 * Also store any pressed modifiers as bound. This prevents
	 * applications from seeing and handling the release event for
	 * a modifier key that was part of a keybinding (e.g. Firefox
	 * displays its menu bar for a lone Alt press + release).
	 */
	for (int i = 0; i < pressed_mods.nr_keys; ++i) {
		add_key(&bound, pressed_mods.keys[i]);
	}
}

bool
key_state_corresponding_press_event_was_bound(uint32_t keycode)
{
	return key_present(&bound, keycode);
}

void
key_state_bound_key_remove(uint32_t keycode)
{
	remove_key(&bound, keycode);
}

int
key_state_nr_bound_keys(void)
{
	return bound.nr_keys;
}

int
key_state_nr_pressed_keys(void)
{
	return pressed.nr_keys;
}
