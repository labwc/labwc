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

struct theme {
	int border_width;
	int padding_height;
	int title_height;
	int menu_overlap_x;
	int menu_overlap_y;

	/* colors */
	float window_active_border_color[4];
	float window_inactive_border_color[4];

	float window_toggled_keybinds_color[4];

	float window_active_title_bg_color[4];
	float window_inactive_title_bg_color[4];

	float window_active_label_text_color[4];
	float window_inactive_label_text_color[4];
	enum lab_justification window_label_text_justify;
	enum lab_justification menu_title_text_justify;

	/* button width */
	int window_button_width;

	/* button colors */
	float window_active_button_menu_unpressed_image_color[4];
	float window_active_button_iconify_unpressed_image_color[4];
	float window_active_button_max_unpressed_image_color[4];
	float window_active_button_close_unpressed_image_color[4];
	float window_active_button_shade_unpressed_image_color[4];
	float window_inactive_button_menu_unpressed_image_color[4];
	float window_inactive_button_iconify_unpressed_image_color[4];
	float window_inactive_button_max_unpressed_image_color[4];
	float window_inactive_button_close_unpressed_image_color[4];
	float window_inactive_button_shade_unpressed_image_color[4];
	/* TODO: add pressed and hover colors for buttons */

	int menu_item_padding_x;
	int menu_item_padding_y;
	int menu_item_height;

	float menu_items_bg_color[4];
	float menu_items_text_color[4];
	float menu_items_active_bg_color[4];
	float menu_items_active_text_color[4];

	int menu_min_width;
	int menu_max_width;

	int menu_separator_line_thickness;
	int menu_separator_padding_width;
	int menu_separator_padding_height;
	float menu_separator_color[4];

	float menu_title_bg_color[4];

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

	/* window drop-shadows */
	int window_active_shadow_size;
	int window_inactive_shadow_size;
	float window_active_shadow_color[4];
	float window_inactive_shadow_color[4];

	/* textures */
	struct lab_data_buffer *button_close_active_unpressed;
	struct lab_data_buffer *button_maximize_active_unpressed;
	struct lab_data_buffer *button_restore_active_unpressed;
	struct lab_data_buffer *button_iconify_active_unpressed;
	struct lab_data_buffer *button_menu_active_unpressed;
	struct lab_data_buffer *button_shade_active_unpressed;
	struct lab_data_buffer *button_unshade_active_unpressed;

	struct lab_data_buffer *button_close_inactive_unpressed;
	struct lab_data_buffer *button_maximize_inactive_unpressed;
	struct lab_data_buffer *button_restore_inactive_unpressed;
	struct lab_data_buffer *button_iconify_inactive_unpressed;
	struct lab_data_buffer *button_menu_inactive_unpressed;
	struct lab_data_buffer *button_shade_inactive_unpressed;
	struct lab_data_buffer *button_unshade_inactive_unpressed;

	/* hover variants are optional and may be NULL */
	struct lab_data_buffer *button_close_active_hover;
	struct lab_data_buffer *button_maximize_active_hover;
	struct lab_data_buffer *button_restore_active_hover;
	struct lab_data_buffer *button_iconify_active_hover;
	struct lab_data_buffer *button_menu_active_hover;
	struct lab_data_buffer *button_shade_active_hover;
	struct lab_data_buffer *button_unshade_active_hover;

	struct lab_data_buffer *button_close_inactive_hover;
	struct lab_data_buffer *button_maximize_inactive_hover;
	struct lab_data_buffer *button_restore_inactive_hover;
	struct lab_data_buffer *button_iconify_inactive_hover;
	struct lab_data_buffer *button_menu_inactive_hover;
	struct lab_data_buffer *button_shade_inactive_hover;
	struct lab_data_buffer *button_unshade_inactive_hover;

	struct lab_data_buffer *corner_top_left_active_normal;
	struct lab_data_buffer *corner_top_right_active_normal;
	struct lab_data_buffer *corner_top_left_inactive_normal;
	struct lab_data_buffer *corner_top_right_inactive_normal;

	struct lab_data_buffer *shadow_corner_top_active;
	struct lab_data_buffer *shadow_corner_bottom_active;
	struct lab_data_buffer *shadow_edge_active;
	struct lab_data_buffer *shadow_corner_top_inactive;
	struct lab_data_buffer *shadow_corner_bottom_inactive;
	struct lab_data_buffer *shadow_edge_inactive;

	/* not set in rc.xml/themerc, but derived from font & padding_height */
	int osd_window_switcher_item_height;

	/* magnifier */
	float mag_border_color[4];
	int mag_border_width;
};

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
