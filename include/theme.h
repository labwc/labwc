/*
 * Theme engine for labwc - trying to be consistent with openbox
 *
 * Copyright Johan Malm 2020
 */

#ifndef THEME_H
#define THEME_H

#include <stdio.h>
#include <wlr/render/wlr_renderer.h>

struct theme {
	float window_active_title_bg_color[4];
	float window_active_handle_bg_color[4];
	float window_inactive_title_bg_color[4];
	struct wlr_texture *xbm_close;
};

extern struct theme theme;

void theme_read(const char *filename);

#endif /* THEME_H */
