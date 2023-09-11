/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_KEYBIND_H
#define LABWC_KEYBIND_H

#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

#define MAX_KEYSYMS 32
#define MAX_KEYCODES 16

struct server;

struct keybind {
	uint32_t modifiers;
	xkb_keysym_t *keysyms;
	size_t keysyms_len;
	bool use_syms_only;
	xkb_keycode_t keycodes[MAX_KEYCODES];
	size_t keycodes_len;
	int keycodes_layout;
	struct wl_list actions;  /* struct action.link */
	struct wl_list link;     /* struct rcxml.keybinds */
};

/**
 * keybind_create - parse keybind and add to linked list
 * @keybind: key combination
 */
struct keybind *keybind_create(const char *keybind);

/**
 * parse_modifier - parse a string containing a single modifier name (e.g. "S")
 * into the represented modifier value. returns 0 for invalid modifier names.
 * @symname: modifier name
 */
uint32_t parse_modifier(const char *symname);

bool keybind_the_same(struct keybind *a, struct keybind *b);

void keybind_update_keycodes(struct server *server);
#endif /* LABWC_KEYBIND_H */
