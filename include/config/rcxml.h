/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RCXML_H
#define LABWC_RCXML_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>
#include <libxml/tree.h>

#include "common/border.h"
#include "common/font.h"
#include "common/node-type.h"
#include "config/types.h"

#define BUTTON_MAP_MAX 16

/* max of one button of each type (no repeats) */
#define TITLE_BUTTONS_MAX ((LAB_NODE_BUTTON_LAST + 1) - LAB_NODE_BUTTON_FIRST)

enum adaptive_sync_mode {
	LAB_ADAPTIVE_SYNC_DISABLED,
	LAB_ADAPTIVE_SYNC_ENABLED,
	LAB_ADAPTIVE_SYNC_FULLSCREEN,
};

enum resize_indicator_mode {
	LAB_RESIZE_INDICATOR_NEVER = 0,
	LAB_RESIZE_INDICATOR_ALWAYS,
	LAB_RESIZE_INDICATOR_NON_PIXEL
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

struct buf;

struct button_map_entry {
	uint32_t from;
	uint32_t to;
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
	bool hide_maximized_window_titlebar;
	int gap;
	enum adaptive_sync_mode adaptive_sync;
	enum tearing_mode allow_tearing;
	bool auto_enable_outputs;
	bool reuse_output_mode;
	bool xwayland_persistence;
	bool primary_selection;
	char *prompt_command;

	/* placement */
	enum lab_placement_policy placement_policy;
	int placement_cascade_offset_x;
	int placement_cascade_offset_y;

	/* focus */
	bool focus_follow_mouse;
	bool focus_follow_mouse_requires_movement;
	bool raise_on_focus;

	/* theme */
	char *theme_name;
	char *icon_theme_name;
	char *fallback_app_icon_name;

	enum lab_node_type title_buttons_left[TITLE_BUTTONS_MAX];
	int nr_title_buttons_left;
	enum lab_node_type title_buttons_right[TITLE_BUTTONS_MAX];
	int nr_title_buttons_right;

	int corner_radius;
	bool show_title;
	bool title_layout_loaded;
	bool ssd_keep_border;
	bool shadows_enabled;
	bool shadows_on_tiled;
	struct font font_activewindow;
	struct font font_inactivewindow;
	struct font font_menuheader;
	struct font font_menuitem;
	struct font font_osd;

	/* Pointer to current theme */
	struct theme *theme;

	/* <margin top="" bottom="" left="" right="" output="" /> */
	struct wl_list usable_area_overrides;

	/* keyboard */
	int repeat_rate;
	int repeat_delay;
	enum lab_tristate kb_numlock_enable;
	bool kb_layout_per_window;
	struct wl_list keybinds;   /* struct keybind.link */

	/* mouse */
	long doubleclick_time;     /* in ms */
	struct wl_list mousebinds; /* struct mousebind.link */

	/* touch tablet */
	struct wl_list touch_configs;

	/* graphics tablet */
	struct tablet_config {
		bool force_mouse_emulation;
		char *output_name;
		struct wlr_fbox box;
		enum lab_rotation rotation;
		uint16_t button_map_count;
		struct button_map_entry button_map[BUTTON_MAP_MAX];
	} tablet;
	struct tablet_tool_config {
		enum lab_motion motion;
		double relative_motion_sensitivity;
	} tablet_tool;

	/* libinput */
	struct wl_list libinput_categories;

	/* resistance */
	int screen_edge_strength;
	int window_edge_strength;
	int unsnap_threshold;
	int unmaximize_threshold;

	/* window snapping */
	int snap_edge_range_inner;
	int snap_edge_range_outer;
	int snap_edge_corner_range;
	bool snap_overlay_enabled;
	int snap_overlay_delay_inner;
	int snap_overlay_delay_outer;
	bool snap_top_maximize;
	enum tiling_events_mode snap_tiling_events_mode;

	enum resize_indicator_mode resize_indicator;
	bool resize_draw_contents;
	int resize_corner_range;
	int resize_minimum_area;

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
		bool unshade;
		enum lab_view_criteria criteria;
		struct wl_list fields;  /* struct window_switcher_field.link */
		enum cycle_osd_style style;
		enum cycle_osd_output_criteria output_criteria;
		char *thumbnail_label_format;
		enum window_switcher_order order;
	} window_switcher;

	struct wl_list window_rules; /* struct window_rule.link */

	/* Menu */
	unsigned int menu_ignore_button_release_period;
	bool menu_show_icons;

	/* Magnifier */
	int mag_width;
	int mag_height;
	float mag_scale;
	float mag_increment;
	bool mag_filter;
};

extern struct rcxml rc;

void rcxml_read(const char *filename);
void rcxml_finish(void);

/*
 * Parse the child <action> nodes and append them to the list.
 * FIXME: move this function to somewhere else.
 */
void append_parsed_actions(xmlNode *node, struct wl_list *list);

#endif /* LABWC_RCXML_H */
