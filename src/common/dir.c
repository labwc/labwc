// SPDX-License-Identifier: GPL-2.0-only
/*
 * Find the configuration and theme directories
 *
 * Copyright Johan Malm 2020
 */
#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "common/dir.h"
#include "common/buf.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "labwc.h"

struct dir {
	const char *prefix;
	const char *default_prefix;
	const char *path;
};

static struct dir config_dirs[] = {
	{
		.prefix = "XDG_CONFIG_HOME",
		.default_prefix = "$HOME/.config",
		.path = "labwc"
	}, {
		.prefix = "XDG_CONFIG_DIRS",
		.default_prefix = "/etc/xdg",
		.path = "labwc",
	}, {
		.path = NULL,
	}
};

static struct dir theme_dirs[] = {
	{
		.prefix = "XDG_DATA_HOME",
		.default_prefix = "$HOME/.local/share",
		.path = "themes",
	}, {
		.prefix = "HOME",
		.path = ".themes",
	}, {
		.prefix = "XDG_DATA_DIRS",
		.default_prefix = "/usr/share:/usr/local/share:/opt/share",
		.path = "themes",
	}, {
		.path = NULL,
	}
};

struct ctx {
	void (*build_path_fn)(struct ctx *ctx, char *prefix, const char *path);
	const char *filename;
	char *buf;
	size_t len;
	struct dir *dirs;
	const char *theme_name;
	struct wl_list *list;
};

struct wl_list *paths_get_prev(struct wl_list *elm) { return elm->prev; }
struct wl_list *paths_get_next(struct wl_list *elm) { return elm->next; }

static void
build_config_path(struct ctx *ctx, char *prefix, const char *path)
{
	assert(prefix);
	snprintf(ctx->buf, ctx->len, "%s/%s/%s", prefix, path, ctx->filename);
}

static void
build_theme_path(struct ctx *ctx, char *prefix, const char *path)
{
	assert(prefix);
	snprintf(ctx->buf, ctx->len, "%s/%s/%s/openbox-3/%s", prefix, path,
		ctx->theme_name, ctx->filename);
}

static void
find_dir(struct ctx *ctx)
{
	char *debug = getenv("LABWC_DEBUG_DIR_CONFIG_AND_THEME");

	struct buf prefix = BUF_INIT;
	for (int i = 0; ctx->dirs[i].path; i++) {
		struct dir d = ctx->dirs[i];
		buf_clear(&prefix);

		/*
		 * Replace (rather than augment) $HOME/.config with
		 * $XDG_CONFIG_HOME if defined, and so on for the other
		 * XDG Base Directories.
		 */
		char *pfxenv = getenv(d.prefix);
		buf_add(&prefix, pfxenv ? pfxenv : d.default_prefix);
		if (!prefix.len) {
			continue;
		}

		/* Handle .default_prefix shell variables such as $HOME */
		buf_expand_shell_variables(&prefix);

		/*
		 * Respect that $XDG_DATA_DIRS can contain multiple colon
		 * separated paths and that we have structured the
		 * .default_prefix in the same way.
		 */
		gchar * *prefixes;
		prefixes = g_strsplit(prefix.data, ":", -1);
		for (gchar * *p = prefixes; *p; p++) {
			ctx->build_path_fn(ctx, *p, d.path);
			if (debug) {
				fprintf(stderr, "%s\n", ctx->buf);
			}

			/*
			 * TODO: We could stat() and continue here if we really
			 * wanted to only respect only the first hit, but feels
			 * like it is probably overkill.
			 */
			struct path *path = znew(*path);
			path->string = xstrdup(ctx->buf);
			wl_list_append(ctx->list, &path->link);
		}
		g_strfreev(prefixes);
	}
	buf_reset(&prefix);
}

void
paths_config_create(struct wl_list *paths, const char *filename)
{
	char buf[4096] = { 0 };
	wl_list_init(paths);

	/*
	 * If user provided a config directory with the -C command line option,
	 * then that trumps everything else and we do not create the
	 * XDG-Base-Dir list.
	 */
	if (rc.config_dir) {
		struct path *path = znew(*path);
		path->string = strdup_printf("%s/%s", rc.config_dir, filename);
		wl_list_append(paths, &path->link);
		return;
	}

	struct ctx ctx = {
		.build_path_fn = build_config_path,
		.filename = filename,
		.buf = buf,
		.len = sizeof(buf),
		.dirs = config_dirs,
		.list = paths,
	};
	find_dir(&ctx);
}

void
paths_theme_create(struct wl_list *paths, const char *theme_name,
		const char *filename)
{
	static char buf[4096] = { 0 };
	wl_list_init(paths);
	struct ctx ctx = {
		.build_path_fn = build_theme_path,
		.filename = filename,
		.buf = buf,
		.len = sizeof(buf),
		.dirs = theme_dirs,
		.theme_name = theme_name,
		.list = paths,
	};
	find_dir(&ctx);
}

void
paths_destroy(struct wl_list *paths)
{
	struct path *path, *next;
	wl_list_for_each_safe(path, next, paths, link) {
		free(path->string);
		wl_list_remove(&path->link);
		free(path);
	}
}
