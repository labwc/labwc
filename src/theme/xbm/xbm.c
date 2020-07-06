/*
 * Create wlr textures based on xbm data
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>

#include "theme/xbm/xbm.h"
#include "theme/xbm/parse.h"

/* built-in 6x6 buttons */
char close_button_normal[] = { 0x33, 0x3f, 0x1e, 0x1e, 0x3f, 0x33 };
char iconify_button_normal[] = { 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f };
char max_button_normal[] = { 0x3f, 0x3f, 0x21, 0x21, 0x21, 0x3f };
char max_button_toggled[] = { 0x3e, 0x22, 0x2f, 0x29, 0x39, 0x0f };

/*
 * TODO: parse rc.xml theme name and look for icons properly.
 *       Just using random icon to prove the point.
 */
static char filename[] = "/usr/share/themes/Bear2/openbox-3/close.xbm";

static struct wlr_texture *texture_from_pixmap(struct wlr_renderer *renderer,
					       struct pixmap *pixmap)
{
	if (!pixmap)
		return NULL;
	return wlr_texture_from_pixels(renderer, WL_SHM_FORMAT_ARGB8888,
				       pixmap->width * 4, pixmap->width,
				       pixmap->height, pixmap->data);
}

static struct wlr_texture *builtin(struct wlr_renderer *renderer,
				   const char *button)
{
	struct pixmap pixmap = xbm_create_pixmap_builtin(button);
	struct wlr_texture *texture = texture_from_pixmap(renderer, &pixmap);
	if (pixmap.data)
		free(pixmap.data);
	return texture;
}

void xbm_load(struct wlr_renderer *renderer)
{
	struct token *tokens;

	char *buffer = xbm_read_file(filename);
	if (!buffer) {
		fprintf(stderr, "no buffer\n");
		goto out;
	}
	tokens = xbm_tokenize(buffer);
	free(buffer);
	struct pixmap pixmap = xbm_create_pixmap(tokens);
	theme.xbm_close = texture_from_pixmap(renderer, &pixmap);
	if (tokens)
		free(tokens);
	if (pixmap.data)
		free(pixmap.data);

out:
	if (!theme.xbm_close)
		theme.xbm_close = builtin(renderer, close_button_normal);
	theme.xbm_maximize = builtin(renderer, max_button_normal);
	theme.xbm_iconify = builtin(renderer, iconify_button_normal);
}
