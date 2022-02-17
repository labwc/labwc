// SPDX-License-Identifier: GPL-2.0-only
/*
 * Create wlr textures based on xbm data
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <drm_fourcc.h>

#include "common/dir.h"
#include "common/grab-file.h"
#include "config/rcxml.h"
#include "theme.h"
#include "xbm/parse.h"
#include "xbm/xbm.h"
#include "buffer.h"

/* built-in 6x6 buttons */
char menu_button_normal[] = { 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00 };
char iconify_button_normal[] = { 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f };
char max_button_normal[] = { 0x3f, 0x3f, 0x21, 0x21, 0x21, 0x3f };
char max_button_toggled[] = { 0x3e, 0x22, 0x2f, 0x29, 0x39, 0x0f };
char close_button_normal[] = { 0x33, 0x3f, 0x1e, 0x1e, 0x3f, 0x33 };

static char *
xbm_path(const char *button)
{
	static char buffer[4096] = { 0 };
	snprintf(buffer, sizeof(buffer), "%s/%s", theme_dir(rc.theme_name),
		 button);
	return buffer;
}

static void
load_button(const char *filename, struct lab_data_buffer **buffer, char *button)
{
	struct pixmap pixmap = {0};
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}

	/* Read file into memory as it's easier to tokenzie that way */
	char *token_buffer = grab_file(xbm_path(filename));
	if (token_buffer) {
		struct token *tokens = tokenize_xbm(token_buffer);
		free(token_buffer);
		pixmap = parse_xbm_tokens(tokens);
		if (tokens) {
			free(tokens);
		}
	}
	if (!pixmap.data) {
		pixmap = parse_xbm_builtin(button, 6);
	}

	/* Create buffer with free_on_destroy being true */
	*buffer = buffer_create_wrap(pixmap.data, pixmap.width, pixmap.height,
		pixmap.width * 4, true);
}

void
xbm_load(struct theme *theme)
{
	parse_set_color(theme->window_active_button_menu_unpressed_image_color);
	load_button("menu.xbm", &theme->xbm_menu_active_unpressed,
		    menu_button_normal);
	parse_set_color(theme->window_active_button_iconify_unpressed_image_color);
	load_button("iconify.xbm", &theme->xbm_iconify_active_unpressed,
		    iconify_button_normal);
	parse_set_color(theme->window_active_button_max_unpressed_image_color);
	load_button("max.xbm", &theme->xbm_maximize_active_unpressed,
		    max_button_normal);
	parse_set_color(theme->window_active_button_close_unpressed_image_color);
	load_button("close.xbm", &theme->xbm_close_active_unpressed,
		    close_button_normal);

	parse_set_color(theme->window_inactive_button_menu_unpressed_image_color);
	load_button("menu.xbm", &theme->xbm_menu_inactive_unpressed,
		    menu_button_normal);
	parse_set_color(theme->window_inactive_button_iconify_unpressed_image_color);
	load_button("iconify.xbm", &theme->xbm_iconify_inactive_unpressed,
		    iconify_button_normal);
	parse_set_color(theme->window_inactive_button_max_unpressed_image_color);
	load_button("max.xbm", &theme->xbm_maximize_inactive_unpressed,
		    max_button_normal);
	parse_set_color(theme->window_inactive_button_close_unpressed_image_color);
	load_button("close.xbm", &theme->xbm_close_inactive_unpressed,
		    close_button_normal);
}
