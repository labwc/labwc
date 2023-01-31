// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/mem.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "config/session.h"

static bool
isfile(const char *path)
{
	struct stat st;
	return (!stat(path, &st));
}

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

static const char *
build_path(const char *dir, const char *filename)
{
	if (string_empty(dir) || string_empty(filename)) {
		return NULL;
	}
	int len = strlen(dir) + strlen(filename) + 2;
	char *buffer = znew_n(char, len);
	strcat(buffer, dir);
	strcat(buffer, "/");
	strcat(buffer, filename);
	return buffer;
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

	char *cmd;
	const char *dbus = "dbus-update-activation-environment ";
	const char *systemd = "systemctl --user import-environment ";

	cmd = znew_n(char, strlen(dbus) + strlen(env_keys) + 1);
	strcat(cmd, dbus);
	strcat(cmd, env_keys);
	spawn_async_no_shell(cmd);
	free(cmd);

	cmd = znew_n(char, strlen(systemd) + strlen(env_keys) + 1);
	strcat(cmd, systemd);
	strcat(cmd, env_keys);
	spawn_async_no_shell(cmd);
	free(cmd);
}

void
session_environment_init(const char *dir)
{
	/*
	 * Set default for XDG_CURRENT_DESKTOP so xdg-desktop-portal-wlr is happy.
	 * May be overriden either by already having a value set or by the user
	 * supplied environment file.
	 */
	setenv("XDG_CURRENT_DESKTOP", "wlroots", 0);

	const char *environment = build_path(dir, "environment");
	if (!environment) {
		return;
	}
	read_environment_file(environment);
	free((void *)environment);
}

void
session_autostart_init(const char *dir)
{
	/* Update dbus and systemd user environment, each may fail gracefully */
	update_activation_env("DISPLAY WAYLAND_DISPLAY XDG_CURRENT_DESKTOP");

	const char *autostart = build_path(dir, "autostart");
	if (!autostart) {
		return;
	}
	if (!isfile(autostart)) {
		wlr_log(WLR_ERROR, "no autostart file");
		goto out;
	}
	wlr_log(WLR_INFO, "run autostart file %s", autostart);
	int len = strlen(autostart) + 4;
	char *cmd = znew_n(char, len);
	strcat(cmd, "sh ");
	strcat(cmd, autostart);
	spawn_async_no_shell(cmd);
	free(cmd);
out:
	free((void *)autostart);
}
