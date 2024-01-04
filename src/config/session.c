// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/file-helpers.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "config/session.h"

static bool
string_empty(const char *s)
{
	return !s || !*s;
}

static void
process_line(char *line)
{
	if (string_empty(line) || line[0] == '#') {
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
	if (string_empty(key) || !value.len) {
		goto error;
	}
	setenv(key, value.buf, 1);
error:
	free(value.buf);
}

static void
read_environment_file(const char *filename)
{
	char *line = NULL;
	size_t len = 0;
	FILE *stream = fopen(filename, "r");
	if (!stream) {
		return;
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
}

static char *
build_path(const char *dir, const char *filename)
{
	if (string_empty(dir) || string_empty(filename)) {
		return NULL;
	}
	return strdup_printf("%s/%s", dir, filename);
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
session_environment_init(const char *dir)
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

	char *environment = build_path(dir, "environment");
	if (!environment) {
		return;
	}
	read_environment_file(environment);
	free(environment);
}

void
session_autostart_init(const char *dir)
{
	/* Update dbus and systemd user environment, each may fail gracefully */
	update_activation_env("DISPLAY WAYLAND_DISPLAY XDG_CURRENT_DESKTOP");

	char *autostart = build_path(dir, "autostart");
	if (!autostart) {
		return;
	}
	if (!file_exists(autostart)) {
		wlr_log(WLR_ERROR, "no autostart file");
		goto out;
	}
	wlr_log(WLR_INFO, "run autostart file %s", autostart);
	char *cmd = strdup_printf("sh %s", autostart);
	spawn_async_no_shell(cmd);
	free(cmd);
out:
	free(autostart);
}
