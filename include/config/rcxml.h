#ifndef __LABWC_RCXML_H
#define __LABWC_RCXML_H

#include <stdbool.h>
#include <stdio.h>
#include <wayland-server-core.h>

#include "common/buf.h"

struct rcxml {
	bool xdg_shell_server_side_deco;
	int gap;
	bool focus_follow_mouse;
	bool raise_on_focus;
	char *theme_name;
	int corner_radius;
	char *font_name_activewindow;
	char *font_name_menuitem;
	int font_size_activewindow;
	int font_size_menuitem;
	struct wl_list keybinds;
};

extern struct rcxml rc;

void rcxml_parse_xml(struct buf *b);
void rcxml_read(const char *filename);
void rcxml_finish(void);

#endif /* __LABWC_RCXML_H */
