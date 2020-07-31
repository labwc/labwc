#ifndef RCXML_H
#define RCXML_H

#include <stdio.h>
#include <stdbool.h>
#include <wlr/types/wlr_keyboard.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>

#include "common/buf.h"

struct keybind {
	uint32_t modifiers;
	xkb_keysym_t *keysyms;
	size_t keysyms_len;
	char *action;
	char *command;
	struct wl_list link;
};

struct keybind *keybind_add(const char *keybind);

struct rcxml {
	bool client_side_decorations;
	char *theme_name;
	char *font_name_activewindow;
	int font_size_activewindow;
	struct wl_list keybinds;
};

extern struct rcxml rc;

void rcxml_parse_xml(struct buf *b);
void rcxml_read(const char *filename);
void rcxml_get_nodenames(struct buf *b);

#endif /* RCXML_H */
