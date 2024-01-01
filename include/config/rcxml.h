/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RCXML_H
#define LABWC_RCXML_H

#include <stdbool.h>
#include <stdio.h>
#include <wayland-server-core.h>

#include "common/border.h"
#include "common/buf.h"
#include "common/font.h"
#include "config/tablet.h"
#include "config/libinput.h"
#include "resize_indicator.h"
#include "theme.h"

enum window_switcher_field_content {
	LAB_FIELD_NONE = 0,
	LAB_FIELD_TYPE,
	LAB_FIELD_IDENTIFIER,
	LAB_FIELD_TRIMMED_IDENTIFIER,
	LAB_FIELD_TITLE,
};

enum view_placement_policy {
	LAB_PLACE_CENTER = 0,
	LAB_PLACE_CURSOR,
	LAB_PLACE_AUTOMATIC
};

enum adaptive_sync_mode {
	LAB_ADAPTIVE_SYNC_DISABLED,
	LAB_ADAPTIVE_SYNC_ENABLED,
	LAB_ADAPTIVE_SYNC_FULLSCREEN,
};

enum tearing_mode {
	LAB_TEARING_DISABLED,
	LAB_TEARING_ENABLED,
	LAB_TEARING_FULLSCREEN,
};

struct usable_area_override {
	struct border margin;
	char *output;
	struct wl_list link; /* struct rcxml.usable_area_overrides */
};

struct window_switcher_field {
	enum window_switcher_field_content content;
	int width;
	struct wl_list link; /* struct rcxml.window_switcher.fields */
};

struct rcxml {
	char *config_dir;

	/* core */
	bool xdg_shell_server_side_deco;
	int gap;
	enum adaptive_sync_mode adaptive_sync;
	enum tearing_mode allow_tearing;
	bool reuse_output_mode;
	enum view_placement_policy placement_policy;

	/* focus */
	bool focus_follow_mouse;
	bool focus_follow_mouse_requires_movement;
	bool raise_on_focus;

	/* theme */
	char *theme_name;
	int corner_radius;
	bool ssd_keep_border;
	struct font font_activewindow;
	struct font font_inactivewindow;
	struct font font_menuitem;
	struct font font_osd;
	/* Pointer to current theme */
	struct theme *theme;

	/* <margin top="" bottom="" left="" right="" output="" /> */
	struct wl_list usable_area_overrides;

	/* keyboard */
	int repeat_rate;
	int repeat_delay;
	bool kb_numlock_enable;
	bool kb_layout_per_window;
	struct wl_list keybinds;   /* struct keybind.link */

	/* mouse */
	long doubleclick_time;     /* in ms */
	struct wl_list mousebinds; /* struct mousebind.link */
	double scroll_factor;

	/* graphics tablet */
	struct tablet_config {
		enum rotation rotation;
		uint16_t button_map_count;
		struct button_map_entry button_map[BUTTON_MAP_MAX];
	} tablet;

	/* libinput */
	struct wl_list libinput_categories;

	/* resistance */
	int screen_edge_strength;

	/* window snapping */
	int snap_edge_range;
	bool snap_top_maximize;

	enum resize_indicator_mode resize_indicator;

	struct {
		int popuptime;
		int min_nr_workspaces;
		struct wl_list workspaces;  /* struct workspace.link */
	} workspace_config;

	/* Regions */
	struct wl_list regions;  /* struct region.link */

	struct {
		bool show;
		bool preview;
		bool outlines;
		struct wl_list fields;  /* struct window_switcher_field.link */
	} window_switcher;

	struct wl_list window_rules; /* struct window_rule.link */
};

extern struct rcxml rc;

void rcxml_parse_xml(struct buf *b);
void rcxml_read(const char *filename);
void rcxml_finish(void);

#endif /* LABWC_RCXML_H */
