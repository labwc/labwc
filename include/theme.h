/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Theme engine for labwc
 *
 * Copyright Johan Malm 2020-2021
 */

#ifndef LABWC_THEME_H
#define LABWC_THEME_H

#include <cairo.h>
#include <stdbool.h>
#include "common/node-type.h"

struct lab_img;

/*
 * Openbox defines 7 types of Gradient background in addition to Solid.
 * Currently, labwc supports only Vertical and SplitVertical.
 */
enum lab_gradient {
	LAB_GRADIENT_NONE, /* i.e. Solid */
	LAB_GRADIENT_VERTICAL,
	LAB_GRADIENT_SPLITVERTICAL,
};

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
	LAB_BS_DEFAULT = 0,

	LAB_BS_HOVERED = 1 << 0,
	LAB_BS_TOGGLED = 1 << 1,
	LAB_BS_ROUNDED = 1 << 2,

	LAB_BS_ALL = LAB_BS_HOVERED | LAB_BS_TOGGLED | LAB_BS_ROUNDED,
};

struct theme_background {
	/* gradient type or none/solid */
	enum lab_gradient gradient;
	/* gradient stops */
	float color[4];
	float color_split_to[4];
	float color_to[4];
	float color_to_split_to[4];
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
	 * ssd_active_state.
	 */
	struct {
		/* title background pattern (solid or gradient) */
		struct theme_background title_bg;

		/* TODO: add toggled/hover/pressed/disabled colors for buttons */
		float button_colors[LAB_NODE_BUTTON_LAST + 1][4];

		float border_color[4];
		float toggled_keybinds_color[4];
		float label_text_color[4];

		/* window drop-shadows */
		int shadow_size;
		float shadow_color[4];

		/*
		 * The texture of a window buttons for each hover/toggled/rounded
		 * state. This can be accessed like:
		 *
		 * buttons[LAB_NODE_BUTTON_ICONIFY][LAB_BS_HOVERED | LAB_BS_TOGGLED]
		 *
		 * Elements in buttons[0] are all NULL since LAB_NODE_BUTTON_FIRST is 1.
		 */
		struct lab_img *button_imgs
			[LAB_NODE_BUTTON_LAST + 1][LAB_BS_ALL + 1];

		/*
		 * The titlebar background is specified as a cairo_pattern
		 * and then also rendered into a 1px wide buffer, which is
		 * stretched horizontally across the titlebar.
		 *
		 * This approach enables vertical gradients while saving
		 * some memory vs. rendering the entire titlebar into an
		 * image. It does not work for horizontal gradients.
		 */
		cairo_pattern_t *titlebar_pattern;
		struct lab_data_buffer *titlebar_fill;

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
	int menu_border_width;
	float menu_border_color[4];

	int menu_items_padding_x;
	int menu_items_padding_y;
	float menu_items_bg_color[4];
	float menu_items_text_color[4];
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

	struct window_switcher_classic_theme {
		int width;
		int padding;
		int item_padding_x;
		int item_padding_y;
		int item_active_border_width;
		float item_active_border_color[4];
		float item_active_bg_color[4];
		int item_icon_size;
		bool width_is_percent;

		/*
		 * Not set in rc.xml/themerc, but derived from the tallest
		 * titlebar object plus 2 * window_titlebar_padding_height
		 */
		int item_height;
	} osd_window_switcher_classic;

	struct window_switcher_thumbnail_theme {
		int max_width;
		int padding;
		int item_width;
		int item_height;
		int item_padding;
		int item_active_border_width;
		float item_active_border_color[4];
		float item_active_bg_color[4];
		int item_icon_size;
		bool max_width_is_percent;

		int title_height;
	} osd_window_switcher_thumbnail;

	int osd_window_switcher_preview_border_width;
	float osd_window_switcher_preview_border_color[3][4];

	int osd_workspace_switcher_boxes_width;
	int osd_workspace_switcher_boxes_height;
	int osd_workspace_switcher_boxes_border_width;

	struct theme_snapping_overlay
		snapping_overlay_region, snapping_overlay_edge;

	/* magnifier */
	float mag_border_color[4];
	int mag_border_width;
};

struct server;

/**
 * theme_init - read openbox theme and generate button textures
 * @theme: theme data
 * @server: server
 * @theme_name: theme-name in <theme-dir>/<theme-name>/labwc/themerc
 * Note <theme-dir> is obtained in theme-dir.c
 */
void theme_init(struct theme *theme, struct server *server, const char *theme_name);

/**
 * theme_finish - free button textures
 * @theme: theme data
 */
void theme_finish(struct theme *theme);

#endif /* LABWC_THEME_H */
