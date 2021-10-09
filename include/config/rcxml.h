#ifndef __LABWC_RCXML_H
#define __LABWC_RCXML_H

#include <stdbool.h>
#include <stdio.h>
#include <wayland-server-core.h>
#include <libinput.h>

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
	struct wl_list mousebinds;
	long doubleclick_time; /* in ms */
	float pointer_speed;
	int natural_scroll;
	int left_handed;
	enum libinput_config_tap_state tap;
	enum libinput_config_accel_profile accel_profile;
	enum libinput_config_middle_emulation_state middle_emu;
	enum libinput_config_dwt_state dwt;
};

extern struct rcxml rc;

void rcxml_parse_xml(struct buf *b);
void rcxml_read(const char *filename);
void rcxml_finish(void);

#endif /* __LABWC_RCXML_H */
