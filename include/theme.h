/*
 * Theme engine for labwc - trying to be consistent with openbox
 *
 * Copyright Johan Malm 2020
 */

#ifndef __LABWC_THEME_H
#define __LABWC_THEME_H

#include <stdio.h>
#include <wlr/render/wlr_renderer.h>

struct theme {
	int border_width;

	float window_active_border_color[4];
	float window_inactive_border_color[4];

	float window_active_title_bg_color[4];
	float window_inactive_title_bg_color[4];

	float window_active_button_unpressed_image_color[4];
	float window_inactive_button_unpressed_image_color[4];

	float menu_items_bg_color[4];
	float menu_items_text_color[4];
	float menu_items_active_bg_color[4];
	float menu_items_active_text_color[4];

	struct wlr_texture *xbm_close_active_unpressed;
	struct wlr_texture *xbm_maximize_active_unpressed;
	struct wlr_texture *xbm_iconify_active_unpressed;

	struct wlr_texture *xbm_close_inactive_unpressed;
	struct wlr_texture *xbm_maximize_inactive_unpressed;
	struct wlr_texture *xbm_iconify_inactive_unpressed;
};

/**
 * theme_init - read openbox theme and generate button textures
 * @theme: theme data
 * @renderer: wlr_renderer for creating button textures
 * @theme_name: theme-name in <theme-dir>/<theme-name>/openbox-3/themerc
 * Note <theme-dir> is obtained in theme-dir.c
 */
void theme_init(struct theme *theme, struct wlr_renderer *renderer,
		const char *theme_name);

/**
 * theme_finish - free button textures
 * @theme: theme data
 */
void theme_finish(struct theme *theme);

#endif /* __LABWC_THEME_H */
