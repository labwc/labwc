// SPDX-License-Identifier: GPL-2.0-only
#include <sfdo-desktop.h>
#include <sfdo-icon.h>
#include <sfdo-basedir.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "desktop-entry.h"
#include "img/img.h"

#include "labwc.h"

static const char *debug_libsfdo;

struct sfdo {
	struct sfdo_desktop_ctx *desktop_ctx;
	struct sfdo_icon_ctx *icon_ctx;
	struct sfdo_desktop_db *desktop_db;
	struct sfdo_icon_theme *icon_theme;
};

static void
log_handler(enum sfdo_log_level level, const char *fmt, va_list args, void *tag)
{
	/*
	 * libsfdo info/debug logging is only provided when LABWC_DEBUG_LIBSFDO
	 * is set to avoid disproportionately verbose logging by default for one
	 * particularly sub-system.
	 */
	if (!debug_libsfdo && level > SFDO_LOG_LEVEL_ERROR) {
		return;
	}

	/*
	 * To avoid logging issues with .desktop files as errors, all libsfdo
	 * error-logging is demoted to info level.
	 */
	if (level == SFDO_LOG_LEVEL_ERROR) {
		level = SFDO_LOG_LEVEL_INFO;
	}

	/* add a prefix if the format length is reasonable */
	char buf[256];
	if (snprintf(buf, sizeof(buf), "[%s] %s", (const char *)tag, fmt)
			< (int)sizeof(buf)) {
		fmt = buf;
	}
	/* sfdo_log_level and wlr_log_importance are compatible */
	_wlr_vlog((enum wlr_log_importance)level, fmt, args);
}

void
desktop_entry_init(struct server *server)
{
	struct sfdo *sfdo = znew(*sfdo);

	debug_libsfdo = getenv("LABWC_DEBUG_LIBSFDO");

	struct sfdo_basedir_ctx *basedir_ctx = sfdo_basedir_ctx_create();
	if (!basedir_ctx) {
		goto err_basedir_ctx;
	}
	sfdo->desktop_ctx = sfdo_desktop_ctx_create(basedir_ctx);
	if (!sfdo->desktop_ctx) {
		goto err_desktop_ctx;
	}
	sfdo->icon_ctx = sfdo_icon_ctx_create(basedir_ctx);
	if (!sfdo->icon_ctx) {
		goto err_icon_ctx;
	}

	/* sfdo_log_level and wlr_log_importance are compatible */
	enum sfdo_log_level level =
		(enum sfdo_log_level)wlr_log_get_verbosity();
	sfdo_desktop_ctx_set_log_handler(
		sfdo->desktop_ctx, level, log_handler, "sfdo-desktop");
	sfdo_icon_ctx_set_log_handler(
		sfdo->icon_ctx, level, log_handler, "sfdo-icon");

	sfdo->desktop_db = sfdo_desktop_db_load(sfdo->desktop_ctx, NULL);
	if (!sfdo->desktop_db) {
		goto err_desktop_db;
	}

	/*
	 * We set some relaxed load options to accommodate delinquent themes in
	 * the wild, namely:
	 *
	 * - SFDO_ICON_THEME_LOAD_OPTION_ALLOW_MISSING to "impose less
	 *   restrictions on the format of icon theme files"
	 *
	 * - SFDO_ICON_THEME_LOAD_OPTION_RELAXED to "continue loading even if it
	 *   fails to find a theme or one of its dependencies."
	 */
	int load_options = SFDO_ICON_THEME_LOAD_OPTIONS_DEFAULT
		| SFDO_ICON_THEME_LOAD_OPTION_ALLOW_MISSING
		| SFDO_ICON_THEME_LOAD_OPTION_RELAXED;

	sfdo->icon_theme = sfdo_icon_theme_load(
		sfdo->icon_ctx,
		rc.icon_theme_name, load_options);
	if (!sfdo->icon_theme) {
		goto err_icon_theme;
	}

	/* basedir_ctx is not referenced by other objects */
	sfdo_basedir_ctx_destroy(basedir_ctx);

	server->sfdo = sfdo;
	return;

err_icon_theme:
	sfdo_desktop_db_destroy(sfdo->desktop_db);
err_desktop_db:
	sfdo_icon_ctx_destroy(sfdo->icon_ctx);
err_icon_ctx:
	sfdo_desktop_ctx_destroy(sfdo->desktop_ctx);
err_desktop_ctx:
	sfdo_basedir_ctx_destroy(basedir_ctx);
err_basedir_ctx:
	free(sfdo);
	wlr_log(WLR_ERROR, "Failed to initialize icon loader");
}

void
desktop_entry_finish(struct server *server)
{
	struct sfdo *sfdo = server->sfdo;
	if (!sfdo) {
		return;
	}

	sfdo_icon_theme_destroy(sfdo->icon_theme);
	sfdo_desktop_db_destroy(sfdo->desktop_db);
	sfdo_icon_ctx_destroy(sfdo->icon_ctx);
	sfdo_desktop_ctx_destroy(sfdo->desktop_ctx);
	free(sfdo);
	server->sfdo = NULL;
}

struct icon_ctx {
	char *path;
	enum sfdo_icon_file_format format;
};

/*
 * Return the length of a filename minus any known extension
 */
static size_t
length_without_extension(const char *name)
{
	size_t len = strlen(name);
	if (len >= 4 && name[len - 4] == '.') {
		const char *ext = &name[len - 3];
		if (!strcmp(ext, "png") || !strcmp(ext, "svg")
				|| !strcmp(ext, "xpm")) {
			len -= 4;
		}
	}
	return len;
}

/*
 * Return 0 on success and -1 on error
 * The calling function is responsible for free()ing ctx->path
 */
static int
process_rel_name(struct icon_ctx *ctx, const char *icon_name,
		struct sfdo *sfdo, int size, int scale)
{
	int ret = 0;
	int lookup_options = SFDO_ICON_THEME_LOOKUP_OPTIONS_DEFAULT;
#if !HAVE_RSVG
	lookup_options |= SFDO_ICON_THEME_LOOKUP_OPTION_NO_SVG;
#endif

	/*
	 * Relative icon names are not supposed to include an extension,
	 * but some .desktop files include one anyway. libsfdo does not
	 * allow this in lookups, so strip the extension first.
	 */
	size_t name_len = length_without_extension(icon_name);
	struct sfdo_icon_file *icon_file = sfdo_icon_theme_lookup(
		sfdo->icon_theme, icon_name, name_len, size, scale,
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
	if (str_endswith_ignore_case(icon_name, ".png")) {
		ctx->format = SFDO_ICON_FILE_FORMAT_PNG;
	} else if (str_endswith_ignore_case(icon_name, ".svg")) {
		ctx->format = SFDO_ICON_FILE_FORMAT_SVG;
	} else if (str_endswith_ignore_case(icon_name, ".xpm")) {
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

		/* sfdo_desktop_entry_get_startup_wm_class() asserts against APPLICATION */
		if (sfdo_desktop_entry_get_type(entry) != SFDO_DESKTOP_ENTRY_APPLICATION) {
			continue;
		}

		/* Try desktop entry's StartupWMClass also */
		const char *wm_class =
			sfdo_desktop_entry_get_startup_wm_class(entry, NULL);
		if (wm_class && !strcasecmp(app_id, wm_class)) {
			return entry;
		}
	}

	/* Try matching partial strings - catches GIMP, among others */
	for (size_t i = 0; i < n_entries; i++) {
		struct sfdo_desktop_entry *entry = entries[i];
		const char *desktop_id = sfdo_desktop_entry_get_id(entry, NULL);
		const char *dot = strrchr(desktop_id, '.');
		const char *desktop_id_base = dot ? (dot + 1) : desktop_id;
		int alen = strlen(app_id);
		int dlen = strlen(desktop_id_base);

		if (!strncasecmp(app_id, desktop_id, alen > dlen ? dlen : alen)) {
			return entry;
		}
	}

	return NULL;
}

static struct sfdo_desktop_entry *
get_desktop_entry(struct sfdo *sfdo, const char *app_id)
{
	struct sfdo_desktop_entry *entry = sfdo_desktop_db_get_entry_by_id(
		sfdo->desktop_db, app_id, SFDO_NT);
	if (!entry) {
		entry = get_db_entry_by_id_fuzzy(sfdo->desktop_db, app_id);
	}

	return entry;
}

static enum lab_img_type
convert_img_type(enum sfdo_icon_file_format fmt)
{
	switch (fmt) {
	case SFDO_ICON_FILE_FORMAT_PNG:
		return LAB_IMG_PNG;
	case SFDO_ICON_FILE_FORMAT_SVG:
		return LAB_IMG_SVG;
	case SFDO_ICON_FILE_FORMAT_XPM:
		return LAB_IMG_XPM;
	default:
		abort();
	}
}

struct lab_img *
desktop_entry_load_icon(struct server *server, const char *icon_name, int size, float scale)
{
	/* static analyzer isn't able to detect the NULL check in string_null_or_empty() */
	if (string_null_or_empty(icon_name) || !icon_name) {
		return NULL;
	}

	struct sfdo *sfdo = server->sfdo;
	if (!sfdo) {
		return NULL;
	}

	/*
	 * libsfdo doesn't support loading icons for fractional scales,
	 * so round down and increase the icon size to compensate.
	 */
	int lookup_scale = MAX((int)scale, 1);
	int lookup_size = lroundf(size * scale / lookup_scale);

	struct icon_ctx ctx = {0};
	int ret;
	if (icon_name[0] == '/') {
		ret = process_abs_name(&ctx, icon_name);
	} else {
		ret = process_rel_name(&ctx, icon_name, sfdo, lookup_size, lookup_scale);
	}
	if (ret < 0) {
		wlr_log(WLR_INFO, "failed to load icon file %s", icon_name);
		return NULL;
	}

	wlr_log(WLR_DEBUG, "loading icon file %s", ctx.path);
	struct lab_img *img = lab_img_load(convert_img_type(ctx.format), ctx.path, NULL);

	free(ctx.path);
	return img;
}

struct lab_img *
desktop_entry_load_icon_from_app_id(struct server *server,
		const char *app_id, int size, float scale)
{
	if (string_null_or_empty(app_id)) {
		return NULL;
	}

	struct sfdo *sfdo = server->sfdo;
	if (!sfdo) {
		return NULL;
	}

	const char *icon_name = NULL;
	struct sfdo_desktop_entry *entry = get_desktop_entry(sfdo, app_id);
	if (entry) {
		icon_name = sfdo_desktop_entry_get_icon(entry, NULL);
	}

	struct lab_img *img = desktop_entry_load_icon(server, icon_name, size, scale);
	if (!img) {
		/* Icon not defined in .desktop file or could not be loaded */
		img = desktop_entry_load_icon(server, app_id, size, scale);
	}
	return img;
}

const char *
desktop_entry_name_lookup(struct server *server, const char *app_id)
{
	if (string_null_or_empty(app_id)) {
		return NULL;
	}

	struct sfdo *sfdo = server->sfdo;
	if (!sfdo) {
		return NULL;
	}

	struct sfdo_desktop_entry *entry = get_desktop_entry(sfdo, app_id);
	if (!entry) {
		return NULL;
	}

	size_t len;
	const char *name = sfdo_desktop_entry_get_name(entry, &len);
	if (!len) {
		return NULL;
	}

	return name;
}
