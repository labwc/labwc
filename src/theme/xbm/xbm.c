/*
 * Create wlr textures based on xbm data
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>

#include "theme/xbm/xbm.h"
#include "theme/xbm/parse.h"

static char filename[] = "/usr/share/themes/Bear2/openbox-3/close.xbm";

void xbm_load(struct wlr_renderer *renderer)
{
	struct token *tokens;

	char *buffer = xbm_read_file(filename);
	if (!buffer) {
		fprintf(stderr, "no buffer\n");
		return;
	}
	tokens = xbm_tokenize(buffer);
	free(buffer);
	struct pixmap pixmap = xbm_create_pixmap(tokens);
	free(tokens);

	theme.xbm_close = wlr_texture_from_pixels(
		renderer, WL_SHM_FORMAT_ARGB8888, pixmap.width * 4,
		pixmap.width, pixmap.height, pixmap.data);
	if (pixmap.data)
		free(pixmap.data);
}
