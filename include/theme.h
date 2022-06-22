/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Theme engine for labwc
 *
 * Copyright Johan Malm 2020-2021
 */

#ifndef __LABWC_THEME_H
#define __LABWC_THEME_H

#include <stdio.h>
#include <wlr/render/wlr_renderer.h>

enum lab_justification {
	LAB_JUSTIFY_LEFT,
	LAB_JUSTIFY_CENTER,
	LAB_JUSTIFY_RIGHT,
};

struct theme {
	int border_width;
	int padding_height;
	int menu_overlap_x;
	int menu_overlap_y;

	/* colors */
	float window_active_border_color[4];
	float window_inactive_border_color[4];

	float window_active_title_bg_color[4];
	float window_inactive_title_bg_color[4];

	float window_active_label_text_color[4];
	float window_inactive_label_text_color[4];
	enum lab_justification window_label_text_justify;

	/* button colors */
	float window_active_button_menu_unpressed_image_color[4];
	float window_active_button_iconify_unpressed_image_color[4];
	float window_active_button_max_unpressed_image_color[4];
	float window_active_button_close_unpressed_image_color[4];
	float window_inactive_button_menu_unpressed_image_color[4];
	float window_inactive_button_iconify_unpressed_image_color[4];
	float window_inactive_button_max_unpressed_image_color[4];
	float window_inactive_button_close_unpressed_image_color[4];
	/* TODO: add pressed and hover colors for buttons */

	float menu_items_bg_color[4];
	float menu_items_text_color[4];
	float menu_items_active_bg_color[4];
	float menu_items_active_text_color[4];

	int menu_separator_width;
	int menu_separator_padding_width;
	int menu_separator_padding_height;
	float menu_separator_color[4];

	int osd_border_width;
	float osd_bg_color[4];
	float osd_border_color[4];
	float osd_label_text_color[4];

	/* textures */
	struct lab_data_buffer *xbm_close_active_unpressed;
	struct lab_data_buffer *xbm_maximize_active_unpressed;
	struct lab_data_buffer *xbm_iconify_active_unpressed;
	struct lab_data_buffer *xbm_menu_active_unpressed;

	struct lab_data_buffer *xbm_close_inactive_unpressed;
	struct lab_data_buffer *xbm_maximize_inactive_unpressed;
	struct lab_data_buffer *xbm_iconify_inactive_unpressed;
	struct lab_data_buffer *xbm_menu_inactive_unpressed;

	struct lab_data_buffer *corner_top_left_active_normal;
	struct lab_data_buffer *corner_top_right_active_normal;
	struct lab_data_buffer *corner_top_left_inactive_normal;
	struct lab_data_buffer *corner_top_right_inactive_normal;

	/* not set in rc.xml/themerc, but derived from font & padding_height */
	int title_height;
};

/**
 * theme_init - read openbox theme and generate button textures
 * @theme: theme data
 * @theme_name: theme-name in <theme-dir>/<theme-name>/openbox-3/themerc
 * Note <theme-dir> is obtained in theme-dir.c
 */
void theme_init(struct theme *theme, const char *theme_name);

/**
 * theme_finish - free button textures
 * @theme: theme data
 */
void theme_finish(struct theme *theme);

#endif /* __LABWC_THEME_H */
