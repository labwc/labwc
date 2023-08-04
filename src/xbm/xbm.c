// SPDX-License-Identifier: GPL-2.0-only
/*
 * Create wlr textures based on xbm data
 *
 * Copyright Johan Malm 2020-2023
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

static char *
xbm_path(const char *button)
{
	static char buffer[4096] = { 0 };
	snprintf(buffer, sizeof(buffer), "%s/%s",
		theme_dir(rc.theme_name), button);
	return buffer;
}

void
xbm_load_button(const char *filename, struct lab_data_buffer **buffer,
		char *fallback_button, float *rgba)
{
	struct pixmap pixmap = {0};
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}

	parse_set_color(rgba);

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
		pixmap = parse_xbm_builtin(fallback_button, 6);
	}

	/* Create buffer with free_on_destroy being true */
	*buffer = buffer_create_wrap(pixmap.data, pixmap.width, pixmap.height,
		pixmap.width * 4, true);
}
