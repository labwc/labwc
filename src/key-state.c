// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "key-state.h"

#define MAX_PRESSED_KEYS (16)

struct key_array {
	uint32_t keys[MAX_PRESSED_KEYS];
	int nr_keys;
};

static struct key_array pressed, bound, pressed_sent;

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
	/* pressed_sent = pressed - bound */
	memcpy(pressed_sent.keys, pressed.keys,
		MAX_PRESSED_KEYS * sizeof(uint32_t));
	pressed_sent.nr_keys = pressed.nr_keys;
	for (int i = 0; i < bound.nr_keys; ++i) {
		remove_key(&pressed_sent, bound.keys[i]);
	}
	return pressed_sent.keys;
}

int
key_state_nr_pressed_sent_keycodes(void)
{
	return pressed_sent.nr_keys;
}

void
key_state_set_pressed(uint32_t keycode, bool ispressed)
{
	if (ispressed) {
		add_key(&pressed, keycode);
	} else {
		remove_key(&pressed, keycode);
	}
}

void
key_state_store_pressed_keys_as_bound(void)
{
	memcpy(bound.keys, pressed.keys, MAX_PRESSED_KEYS * sizeof(uint32_t));
	bound.nr_keys = pressed.nr_keys;
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

/*
 * Reference:
 * https://github.com/xkbcommon/libxkbcommon/blob/HEAD/include/xkbcommon/xkbcommon-keysyms.h
 */
bool
key_state_multiple_normal_keys_pressed(void)
{
	if (pressed.nr_keys < 2) {
		return false;
	}

	/*
	 * If the pressed array contains multiple keys, we have to analyze a bit
	 * further because some keys like those listed below do not always
	 * receive release events.
	 *   - XKB_KEY_ISO_Group_Next
	 *   - XKB_KEY_XF86WakeUp
	 *   - XKB_KEY_XF86Suspend
	 *   - XKB_KEY_XF86Hibernate
	 */
	size_t count = 0;
	for (int i = 0; i < pressed.nr_keys; ++i) {
		/* Convert from udev event to xkbcommon code */
		uint32_t keycode = pressed.keys[i] + 8;
		if (keycode >= 0x1000000) {
			/* XFree86 vendor specific keysyms */
			continue;
		} else if (keycode >= 0xFE00 && keycode <= 0xFEFF) {
			/* Extension function and modifier keys */
			continue;
		}
		++count;
	}
	return count > 1;
}
