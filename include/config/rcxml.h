/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RCXML_H
#define LABWC_RCXML_H

#include <stdbool.h>
#include <stdio.h>
#include <wayland-server-core.h>

#include "common/border.h"
#include "common/buf.h"
#include "common/font.h"
#include "config/touch.h"
#include "config/tablet.h"
#include "config/tablet-tool.h"
#include "config/libinput.h"
#include "resize-indicator.h"
#include "ssd.h"
#include "theme.h"

enum view_placement_policy {
	LAB_PLACE_INVALID = 0,
	LAB_PLACE_CENTER,
	LAB_PLACE_CURSOR,
	LAB_PLACE_AUTOMATIC,
	LAB_PLACE_CASCADE,
};

enum adaptive_sync_mode {
	LAB_ADAPTIVE_SYNC_DISABLED,
	LAB_ADAPTIVE_SYNC_ENABLED,
	LAB_ADAPTIVE_SYNC_FULLSCREEN,
};

enum tearing_mode {
	LAB_TEARING_DISABLED = 0,
	LAB_TEARING_ENABLED,
	LAB_TEARING_FULLSCREEN,
	LAB_TEARING_FULLSCREEN_FORCED,
};

enum tiling_events_mode {
	LAB_TILING_EVENTS_NEVER = 0,
	LAB_TILING_EVENTS_REGION = 1 << 0,
	LAB_TILING_EVENTS_EDGE = 1 << 1,
	LAB_TILING_EVENTS_ALWAYS =
		(LAB_TILING_EVENTS_REGION | LAB_TILING_EVENTS_EDGE),
};

struct title_button {
	enum ssd_part_type type;
	struct wl_list link;
};

struct usable_area_override {
	struct border margin;
	char *output;
	struct wl_list link; /* struct rcxml.usable_area_overrides */
};

struct rcxml {
	/* from command line */
	char *config_dir;
	char *config_file;
	bool merge_config;

	/* core */
	bool xdg_shell_server_side_deco;
	int gap;
	enum adaptive_sync_mode adaptive_sync;
	enum tearing_mode allow_tearing;
	bool reuse_output_mode;
	enum view_placement_policy placement_policy;
	bool xwayland_persistence;
	int placement_cascade_offset_x;
	int placement_cascade_offset_y;

	/* focus */
	bool focus_follow_mouse;
	bool focus_follow_mouse_requires_movement;
	bool raise_on_focus;

	/* theme */
	char *theme_name;
	struct wl_list title_buttons_left;
	struct wl_list title_buttons_right;
	int corner_radius;
	bool show_title;
	bool title_layout_loaded;
	bool ssd_keep_border;
	bool shadows_enabled;
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

	/* touch tablet */
	struct wl_list touch_configs;

	/* graphics tablet */
	struct tablet_config {
		bool force_mouse_emulation;
		char *output_name;
		struct wlr_fbox box;
		enum rotation rotation;
		uint16_t button_map_count;
		struct button_map_entry button_map[BUTTON_MAP_MAX];
	} tablet;
	struct tablet_tool_config {
		enum motion motion;
		double relative_motion_sensitivity;
	} tablet_tool;

	/* libinput */
	struct wl_list libinput_categories;

	/* resistance */
	int screen_edge_strength;
	int window_edge_strength;
	int unsnap_threshold;

	/* window snapping */
	int snap_edge_range;
	bool snap_overlay_enabled;
	int snap_overlay_delay_inner;
	int snap_overlay_delay_outer;
	bool snap_top_maximize;
	enum tiling_events_mode snap_tiling_events_mode;

	enum resize_indicator_mode resize_indicator;
	bool resize_draw_contents;

	struct {
		int popuptime;
		int min_nr_workspaces;
		char *prefix;
		struct wl_list workspaces;  /* struct workspace.link */
	} workspace_config;

	/* Regions */
	struct wl_list regions;  /* struct region.link */

	/* Window Switcher */
	struct {
		bool show;
		bool preview;
		bool outlines;
		uint32_t criteria;
		struct wl_list fields;  /* struct window_switcher_field.link */
	} window_switcher;

	struct wl_list window_rules; /* struct window_rule.link */

	/* Menu */
	unsigned int menu_ignore_button_release_period;

	/* Magnifier */
	int mag_width;
	int mag_height;
	float mag_scale;
	float mag_increment;
	bool mag_filter;
};

extern struct rcxml rc;

void rcxml_parse_xml(struct buf *b);
void rcxml_read(const char *filename);
void rcxml_finish(void);

#endif /* LABWC_RCXML_H */
