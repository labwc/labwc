// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/dir.h"
#include "common/file-helpers.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "config/session.h"
#include "labwc.h"

static void
process_line(char *line)
{
	if (string_null_or_empty(line) || line[0] == '#') {
		return;
	}
	char *key = NULL;
	char *p = strchr(line, '=');
	if (!p) {
		return;
	}
	*p = '\0';
	key = string_strip(line);

	struct buf value;
	buf_init(&value);
	buf_add(&value, string_strip(++p));
	buf_expand_shell_variables(&value);
	buf_expand_tilde(&value);
	if (string_null_or_empty(key) || !value.len) {
		goto error;
	}
	setenv(key, value.buf, 1);
error:
	free(value.buf);
}

/* return true on successful read */
static bool
read_environment_file(const char *filename)
{
	char *line = NULL;
	size_t len = 0;
	FILE *stream = fopen(filename, "r");
	if (!stream) {
		return false;
	}
	wlr_log(WLR_INFO, "read environment file %s", filename);
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		process_line(line);
	}
	free(line);
	fclose(stream);
	return true;
}

static void
update_activation_env(const char *env_keys)
{
	if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
		/* Prevent accidentally auto-launching a dbus session */
		wlr_log(WLR_INFO, "Not updating dbus execution environment: "
			"DBUS_SESSION_BUS_ADDRESS not set");
		return;
	}
	wlr_log(WLR_INFO, "Updating dbus execution environment");

	char *cmd = strdup_printf("dbus-update-activation-environment %s", env_keys);
	spawn_async_no_shell(cmd);
	free(cmd);

	cmd = strdup_printf("systemctl --user import-environment %s", env_keys);
	spawn_async_no_shell(cmd);
	free(cmd);
}

void
session_environment_init(void)
{
	/*
	 * Set default for XDG_CURRENT_DESKTOP so xdg-desktop-portal-wlr is happy.
	 * May be overridden either by already having a value set or by the user
	 * supplied environment file.
	 */
	setenv("XDG_CURRENT_DESKTOP", "wlroots", 0);

	/*
	 * Set default for _JAVA_AWT_WM_NONREPARENTING so that Java applications
	 * such as JetBrains/Intellij Idea do render blank windows and menus
	 * with incorrect offset. See https://github.com/swaywm/sway/issues/595
	 * May be overridden either by already having a value set or by the user
	 * supplied environment file.
	 */
	setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 0);

	struct wl_list paths;
	paths_config_create(&paths, "environment");

	bool should_merge_config = rc.merge_config;
	struct wl_list *(*iter)(struct wl_list *list);
	iter = should_merge_config ? paths_get_prev : paths_get_next;

	for (struct wl_list *elm = iter(&paths); elm != &paths; elm = iter(elm)) {
		struct path *path = wl_container_of(elm, path, link);
		bool success = read_environment_file(path->string);
		if (success && !should_merge_config) {
			break;
		}
	}
	paths_destroy(&paths);
}

void
session_autostart_init(void)
{
	/* Update dbus and systemd user environment, each may fail gracefully */
	update_activation_env("DISPLAY WAYLAND_DISPLAY XDG_CURRENT_DESKTOP "
		"XCURSOR_SIZE XCURSOR_THEME XDG_SESSION_TYPE LABWC_PID");

	struct wl_list paths;
	paths_config_create(&paths, "autostart");

	bool should_merge_config = rc.merge_config;
	struct wl_list *(*iter)(struct wl_list *list);
	iter = should_merge_config ? paths_get_prev : paths_get_next;

	for (struct wl_list *elm = iter(&paths); elm != &paths; elm = iter(elm)) {
		struct path *path = wl_container_of(elm, path, link);
		if (!file_exists(path->string)) {
			continue;
		}
		wlr_log(WLR_INFO, "run autostart file %s", path->string);
		char *cmd = strdup_printf("sh %s", path->string);
		spawn_async_no_shell(cmd);
		free(cmd);

		if (!should_merge_config) {
			break;
		}
	}
	paths_destroy(&paths);
}
