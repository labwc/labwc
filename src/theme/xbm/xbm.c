/*
 * Create wlr textures based on xbm data
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "theme/xbm/xbm.h"
#include "theme/xbm/parse.h"
#include "rcxml.h"

struct dir {
	const char *prefix;
	const char *path;
};

static struct dir theme_dirs[] = {
	{ "XDG_DATA_HOME", "themes" },
	{ "HOME", ".local/share/themes" },
	{ "HOME", ".themes" },
	{ "XDG_DATA_HOME", "themes" },
	{ NULL, "/usr/share/themes" },
	{ NULL, "/usr/local/share/themes" },
	{ NULL, "opt/share/themes" },
	{ NULL, NULL }
};

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

static struct wlr_texture *builtin(struct wlr_renderer *renderer,
				   const char *button)
{
	struct pixmap pixmap = xbm_create_pixmap_builtin(button);
	struct wlr_texture *texture = texture_from_pixmap(renderer, &pixmap);
	if (pixmap.data)
		free(pixmap.data);
	return texture;
}

static char *theme_dir(void)
{
	static char buffer[4096] = { 0 };
	if (buffer[0] != '\0')
		return buffer;

	struct stat st;
	for (int i = 0; theme_dirs[i].path; i++) {
		char *prefix = NULL;
		struct dir d = theme_dirs[i];
		if (d.prefix) {
			prefix = getenv(d.prefix);
			if (!prefix)
				continue;
			snprintf(buffer, sizeof(buffer), "%s/%s/%s/openbox-3",
				 prefix, d.path, rc.theme_name);
		} else {
			snprintf(buffer, sizeof(buffer), "%s/%s/openbox-3",
				 d.path, rc.theme_name);
		}
		if (!stat(buffer, &st) && S_ISDIR(st.st_mode))
			return buffer;
	}
	buffer[0] = '\0';
	return buffer;
}

static char *xbm_path(const char *button)
{
	static char buffer[4096] = { 0 };
	snprintf(buffer, sizeof(buffer), "%s/%s", theme_dir(), button);
	return buffer;
}

void xbm_load(struct wlr_renderer *renderer)
{
	struct token *tokens;

	char *buffer = xbm_read_file(xbm_path("close.xbm"));
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
