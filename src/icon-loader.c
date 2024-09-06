// SPDX-License-Identifier: GPL-2.0-only
#include <sfdo-desktop.h>
#include <sfdo-icon.h>
#include <sfdo-basedir.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "config.h"
#include "icon-loader.h"
#include "img/img-png.h"

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
	if (entry) {
		icon_name = sfdo_desktop_entry_get_icon(entry, NULL);
	}
	if (!icon_name) {
		/* fall back to app id */
		icon_name = app_id;
	}

	int lookup_options = SFDO_ICON_THEME_LOOKUP_OPTIONS_DEFAULT;
#if !HAVE_RSVG
	lookup_options |= SFDO_ICON_THEME_LOOKUP_OPTION_NO_SVG;
#endif
	struct sfdo_icon_file *icon_file = sfdo_icon_theme_lookup(
		loader->icon_theme, icon_name, SFDO_NT, size, scale,
		lookup_options);
	if (!icon_file || icon_file == SFDO_ICON_FILE_INVALID) {
		return NULL;
	}

	struct lab_data_buffer *icon_buffer = NULL;
	const char *icon_path = sfdo_icon_file_get_path(icon_file, NULL);

	wlr_log(WLR_DEBUG, "loading icon file %s", icon_path);

	switch (sfdo_icon_file_get_format(icon_file)) {
	case SFDO_ICON_FILE_FORMAT_PNG:
		img_png_load(icon_path, &icon_buffer);
		break;
	case SFDO_ICON_FILE_FORMAT_SVG:
#if HAVE_RSVG
		img_svg_load(icon_path, &icon_buffer, size * scale);
#endif
		break;
	case SFDO_ICON_FILE_FORMAT_XPM:
		/* XPM is not supported */
		break;
	}

	sfdo_icon_file_destroy(icon_file);

	return icon_buffer;
}
