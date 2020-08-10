/*
 * Create wlr textures based on xbm data
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>

#include "theme/theme.h"
#include "theme/xbm/xbm.h"
#include "theme/xbm/parse.h"
#include "config/rcxml.h"
#include "common/dir.h"
#include "common/grab-file.h"

/* built-in 6x6 buttons */
char close_button_normal[] = { 0x33, 0x3f, 0x1e, 0x1e, 0x3f, 0x33 };
char iconify_button_normal[] = { 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f };
char max_button_normal[] = { 0x3f, 0x3f, 0x21, 0x21, 0x21, 0x3f };
char max_button_toggled[] = { 0x3e, 0x22, 0x2f, 0x29, 0x39, 0x0f };

static struct wlr_texture *texture_from_pixmap(struct wlr_renderer *renderer,
					       struct pixmap *pixmap)
{
	if (!pixmap)
		return NULL;
	return wlr_texture_from_pixels(renderer, WL_SHM_FORMAT_ARGB8888,
				       pixmap->width * 4, pixmap->width,
				       pixmap->height, pixmap->data);
}

static struct wlr_texture *texture_from_builtin(struct wlr_renderer *renderer,
						const char *button)
{
	struct pixmap pixmap = parse_xbm_builtin(button);
	struct wlr_texture *texture = texture_from_pixmap(renderer, &pixmap);
	if (pixmap.data)
		free(pixmap.data);
	return texture;
}

static char *xbm_path(const char *button)
{
	static char buffer[4096] = { 0 };
	snprintf(buffer, sizeof(buffer), "%s/%s", theme_dir(rc.theme_name),
		 button);
	return buffer;
}

static void load_button(struct wlr_renderer *renderer, const char *filename,
			struct wlr_texture **texture, char *button)
{
	/* Read file into memory as it's easier to tokenzie that way */
	char *buffer = grab_file(xbm_path(filename));
	if (!buffer)
		goto out;
	fprintf(stderr, "loading %s\n", filename);
	struct token *tokens = tokenize_xbm(buffer);
	free(buffer);

	struct pixmap pixmap = parse_xbm_tokens(tokens);
	*texture = texture_from_pixmap(renderer, &pixmap);
	if (tokens)
		free(tokens);
	if (pixmap.data)
		free(pixmap.data);
out:
	if (!(*texture))
		*texture = texture_from_builtin(renderer, button);
}

void xbm_load(struct wlr_renderer *r)
{
	load_button(r, "close.xbm", &theme.xbm_close, close_button_normal);
	load_button(r, "max.xbm", &theme.xbm_maximize, max_button_normal);
	load_button(r, "iconify.xbm", &theme.xbm_iconify,
		    iconify_button_normal);
}
