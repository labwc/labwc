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
	float window_active_title_bg_color[4];
	float window_active_handle_bg_color[4];
	float window_inactive_title_bg_color[4];
	float window_active_button_unpressed_image_color[4];
	struct wlr_texture *xbm_close;
	struct wlr_texture *xbm_maximize;
	struct wlr_texture *xbm_iconify;
};

extern struct theme theme;

/**
 * theme_read - read theme into global theme struct
 * @theme_name: theme-name in <theme-dir>/<theme-name>/openbox-3/themerc
 * Note <theme-dir> is obtained in theme-dir.c
 */
void theme_read(const char *theme_name);

#endif /* __LABWC_THEME_H */
