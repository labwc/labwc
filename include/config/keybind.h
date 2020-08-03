#ifndef __LABWC_KEYBIND_H
#define __LABWC_KEYBIND_H

#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

struct keybind {
	uint32_t modifiers;
	xkb_keysym_t *keysyms;
	size_t keysyms_len;
	char *action;
	char *command;
	struct wl_list link;
};

struct keybind *keybind_add(const char *keybind);

#endif /* __LABWC_KEYBIND_H */
