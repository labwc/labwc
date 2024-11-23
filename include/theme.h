/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Theme engine for labwc
 *
 * Copyright Johan Malm 2020-2021
 */

#ifndef LABWC_THEME_H
#define LABWC_THEME_H

#include <stdio.h>
#include <wlr/render/wlr_renderer.h>
#include "ssd.h"

enum lab_justification {
	LAB_JUSTIFY_LEFT,
	LAB_JUSTIFY_CENTER,
	LAB_JUSTIFY_RIGHT,
};

struct theme_snapping_overlay {
	bool bg_enabled;
	bool border_enabled;
	float bg_color[4];
	int border_width;
	float border_color[3][4];
};

enum lab_button_state {
	LAB_BS_HOVERD = 1 << 0,
	LAB_BS_TOGGLED = 1 << 1,
	LAB_BS_ROUNDED = 1 << 2,

	LAB_BS_ALL = LAB_BS_HOVERD | LAB_BS_TOGGLED | LAB_BS_ROUNDED,
};

struct theme {
	int border_width;

	/*
	 * the space between title bar border and
	 * buttons on the left/right/top
	 */
	int window_titlebar_padding_width;
	int window_titlebar_padding_height;

	int titlebar_height;

	float window_toggled_keybinds_color[4];
	enum lab_justification window_label_text_justify;

	/* buttons */
	int window_button_width;
	int window_button_height;
	int window_button_spacing;

	/* the corner radius of the hover effect */
	int window_button_hover_bg_corner_radius;

	/*
	 * Themes/textures for each active/inactive window. Indexed by
	 * THEME_INACTIVE and THEME_ACTIVE.
	 */
	struct {
		/* TODO: add toggled/hover/pressed/disabled colors for buttons */
		float button_colors[LAB_SSD_BUTTON_LAST + 1][4];

		float border_color[4];
		float toggled_keybinds_color[4];
		float title_bg_color[4];
		float label_text_color[4];

		/* window drop-shadows */
		int shadow_size;
		float shadow_color[4];

		/*
		 * The texture of a window buttons for each hover/toggled/rounded
		 * state. This can be accessed like:
		 *
		 * buttons[LAB_SSD_BUTTON_ICONIFY][LAB_BS_HOVERD | LAB_BS_TOGGLED]
		 *
		 * Elements in buttons[0] are all NULL since LAB_SSD_BUTTON_FIRST is 1.
		 */
		struct lab_data_buffer *buttons
			[LAB_SSD_BUTTON_LAST + 1][LAB_BS_ALL + 1];

		struct lab_data_buffer *corner_top_left_normal;
		struct lab_data_buffer *corner_top_right_normal;

		struct lab_data_buffer *shadow_corner_top;
		struct lab_data_buffer *shadow_corner_bottom;
		struct lab_data_buffer *shadow_edge;
	} window[2];

	/* Derived from font sizes */
	int menu_item_height;
	int menu_header_height;

	int menu_overlap_x;
	int menu_overlap_y;
	int menu_min_width;
	int menu_max_width;
	int menu_padding_width;
	int menu_padding_height;
	int menu_corner_radius;
	int menu_border_width;
	float menu_border_color[4];
	float menu_bg_color[4];

	int menu_items_padding_x;
	int menu_items_padding_y;
	int menu_items_corner_radius;
	int menu_items_border_width;
	float menu_items_border_color[4];
	float menu_items_bg_color[4];
	float menu_items_text_color[4];
	float menu_items_active_border_color[4];
	float menu_items_active_bg_color[4];
	float menu_items_active_text_color[4];

	int menu_separator_line_thickness;
	int menu_separator_padding_width;
	int menu_separator_padding_height;
	float menu_separator_color[4];

	float menu_title_bg_color[4];
	enum lab_justification menu_title_text_justify;
	float menu_title_text_color[4];

	int osd_border_width;

	float osd_bg_color[4];
	float osd_border_color[4];
	float osd_label_text_color[4];

	int osd_window_switcher_width;
	int osd_window_switcher_padding;
	int osd_window_switcher_item_padding_x;
	int osd_window_switcher_item_padding_y;
	int osd_window_switcher_item_active_border_width;
	bool osd_window_switcher_width_is_percent;
	int osd_window_switcher_preview_border_width;
	float osd_window_switcher_preview_border_color[3][4];

	int osd_workspace_switcher_boxes_width;
	int osd_workspace_switcher_boxes_height;

	struct theme_snapping_overlay
		snapping_overlay_region, snapping_overlay_edge;

	/*
	 * Not set in rc.xml/themerc, but derived from the tallest titlebar
	 * object plus 2 * window_titlebar_padding_height
	 */
	int osd_window_switcher_item_height;

	/* magnifier */
	float mag_border_color[4];
	int mag_border_width;
};

#define THEME_INACTIVE 0
#define THEME_ACTIVE 1

struct server;

/**
 * theme_init - read openbox theme and generate button textures
 * @theme: theme data
 * @server: server
 * @theme_name: theme-name in <theme-dir>/<theme-name>/openbox-3/themerc
 * Note <theme-dir> is obtained in theme-dir.c
 */
void theme_init(struct theme *theme, struct server *server, const char *theme_name);

/**
 * theme_finish - free button textures
 * @theme: theme data
 */
void theme_finish(struct theme *theme);

#endif /* LABWC_THEME_H */
