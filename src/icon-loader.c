// SPDX-License-Identifier: GPL-2.0-only
#include <sfdo-desktop.h>
#include <sfdo-icon.h>
#include <sfdo-basedir.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "common/string-helpers.h"
#include "config.h"
#include "icon-loader.h"
#include "img/img-png.h"
#include "img/img-xpm.h"

#if HAVE_RSVG
#include "img/img-svg.h"
#endif

#include "labwc.h"

struct icon_loader {
	struct sfdo_desktop_ctx *desktop_ctx;
	struct sfdo_icon_ctx *icon_ctx;
	struct sfdo_desktop_db *desktop_db;
	struct sfdo_icon_theme *icon_theme;
};

void
icon_loader_init(struct server *server)
{
	struct icon_loader *loader = znew(*loader);

	struct sfdo_basedir_ctx *basedir_ctx = sfdo_basedir_ctx_create();
	if (!basedir_ctx) {
		goto err_basedir_ctx;
	}
	loader->desktop_ctx = sfdo_desktop_ctx_create(basedir_ctx);
	if (!loader->desktop_ctx) {
		goto err_desktop_ctx;
	}
	loader->icon_ctx = sfdo_icon_ctx_create(basedir_ctx);
	if (!loader->icon_ctx) {
		goto err_icon_ctx;
	}
	loader->desktop_db = sfdo_desktop_db_load(loader->desktop_ctx, NULL);
	if (!loader->desktop_db) {
		goto err_desktop_db;
	}
	loader->icon_theme = sfdo_icon_theme_load(loader->icon_ctx,
		rc.icon_theme_name, SFDO_ICON_THEME_LOAD_OPTIONS_DEFAULT);
	if (!loader->icon_theme) {
		goto err_icon_theme;
	}

	/* basedir_ctx is not referenced by other objects */
	sfdo_basedir_ctx_destroy(basedir_ctx);

	server->icon_loader = loader;
	return;

err_icon_theme:
	sfdo_desktop_db_destroy(loader->desktop_db);
err_desktop_db:
	sfdo_icon_ctx_destroy(loader->icon_ctx);
err_icon_ctx:
	sfdo_desktop_ctx_destroy(loader->desktop_ctx);
err_desktop_ctx:
	sfdo_basedir_ctx_destroy(basedir_ctx);
err_basedir_ctx:
	free(loader);
	wlr_log(WLR_ERROR, "Failed to initialize icon loader");
}

void
icon_loader_finish(struct server *server)
{
	struct icon_loader *loader = server->icon_loader;
	if (!loader) {
		return;
	}

	sfdo_desktop_db_destroy(loader->desktop_db);
	sfdo_icon_ctx_destroy(loader->icon_ctx);
	sfdo_desktop_ctx_destroy(loader->desktop_ctx);
	free(loader);
	server->icon_loader = NULL;
}

struct icon_ctx {
	char *path;
	enum sfdo_icon_file_format format;
};

/*
 * Return 0 on success and -1 on error
 * The calling function is responsible for free()ing ctx->path
 */
static int
process_rel_name(struct icon_ctx *ctx, const char *icon_name,
		struct icon_loader *loader, int size, int scale)
{
	int ret = 0;
	int lookup_options = SFDO_ICON_THEME_LOOKUP_OPTIONS_DEFAULT;
#if !HAVE_RSVG
	lookup_options |= SFDO_ICON_THEME_LOOKUP_OPTION_NO_SVG;
#endif
	struct sfdo_icon_file *icon_file = sfdo_icon_theme_lookup(
		loader->icon_theme, icon_name, SFDO_NT, size, scale,
		lookup_options);
	if (!icon_file || icon_file == SFDO_ICON_FILE_INVALID) {
		ret = -1;
		goto out;
	}
	ctx->path = xstrdup(sfdo_icon_file_get_path(icon_file, NULL));
	ctx->format = sfdo_icon_file_get_format(icon_file);
out:
	sfdo_icon_file_destroy(icon_file);
	return ret;
}

static int
process_abs_name(struct icon_ctx *ctx, const char *icon_name)
{
	ctx->path = xstrdup(icon_name);
	if (str_endswith(icon_name, ".png")) {
		ctx->format = SFDO_ICON_FILE_FORMAT_PNG;
	} else if (str_endswith(icon_name, ".svg")) {
		ctx->format = SFDO_ICON_FILE_FORMAT_SVG;
	} else if (str_endswith(icon_name, ".xpm")) {
		ctx->format = SFDO_ICON_FILE_FORMAT_XPM;
	} else {
		goto err;
	}
	return 0;
err:
	wlr_log(WLR_ERROR, "'%s' has invalid file extension", icon_name);
	free(ctx->path);
	return -1;
}

/*
 * Looks up an application desktop entry using fuzzy matching
 * (e.g. "thunderbird" matches "org.mozilla.Thunderbird.desktop"
 * and "XTerm" matches "xterm.desktop"). This is not per any spec
 * but is needed to find icons for existing applications.
 */
static struct sfdo_desktop_entry *
get_db_entry_by_id_fuzzy(struct sfdo_desktop_db *db, const char *app_id)
{
	size_t n_entries;
	struct sfdo_desktop_entry **entries = sfdo_desktop_db_get_entries(db, &n_entries);

	for (size_t i = 0; i < n_entries; i++) {
		struct sfdo_desktop_entry *entry = entries[i];
		const char *desktop_id = sfdo_desktop_entry_get_id(entry, NULL);
		/* Get portion of desktop ID after last '.' */
		const char *dot = strrchr(desktop_id, '.');
		const char *desktop_id_base = dot ? (dot + 1) : desktop_id;

		if (!strcasecmp(app_id, desktop_id_base)) {
			return entry;
		}
	}

	return NULL;
}

struct lab_data_buffer *
icon_loader_lookup(struct server *server, const char *app_id, int size, int scale)
{
	struct icon_loader *loader = server->icon_loader;
	if (!loader) {
		return NULL;
	}

	const char *icon_name = NULL;
	struct sfdo_desktop_entry *entry = sfdo_desktop_db_get_entry_by_id(
		loader->desktop_db, app_id, SFDO_NT);
	if (!entry) {
		entry = get_db_entry_by_id_fuzzy(loader->desktop_db, app_id);
	}
	if (entry) {
		icon_name = sfdo_desktop_entry_get_icon(entry, NULL);
	}

	struct icon_ctx ctx = {0};
	int ret;
	if (!icon_name) {
		/* fall back to app id */
		ret = process_rel_name(&ctx, app_id, loader, size, scale);
	} else if (icon_name[0] == '/') {
		ret = process_abs_name(&ctx, icon_name);
	} else {
		/* this should be the case for most icons */
		ret = process_rel_name(&ctx, icon_name, loader, size, scale);
	}
	if (ret < 0) {
		return NULL;
	}

	struct lab_data_buffer *icon_buffer = NULL;

	wlr_log(WLR_DEBUG, "loading icon file %s", ctx.path);

	switch (ctx.format) {
	case SFDO_ICON_FILE_FORMAT_PNG:
		img_png_load(ctx.path, &icon_buffer);
		break;
	case SFDO_ICON_FILE_FORMAT_SVG:
#if HAVE_RSVG
		img_svg_load(ctx.path, &icon_buffer, size * scale);
#endif
		break;
	case SFDO_ICON_FILE_FORMAT_XPM:
		img_xpm_load(ctx.path, &icon_buffer);
		break;
	}

	free(ctx.path);
	return icon_buffer;
}
