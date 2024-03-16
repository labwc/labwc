// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wlr/backend/drm.h>
#include <wlr/backend/multi.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/dir.h"
#include "common/file-helpers.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "config/session.h"
#include "labwc.h"

static const char *const env_vars[] = {
	"DISPLAY",
	"WAYLAND_DISPLAY",
	"XDG_CURRENT_DESKTOP",
	"XCURSOR_SIZE",
	"XCURSOR_THEME",
	"XDG_SESSION_TYPE",
	"LABWC_PID",
	NULL
};

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
	if (string_null_or_empty(key)) {
		return;
	}

	struct buf value;
	buf_init(&value);
	buf_add(&value, string_strip(++p));
	buf_expand_shell_variables(&value);
	buf_expand_tilde(&value);
	setenv(key, value.buf, 1);
	buf_finish(&value);
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

static char *
strdup_env_path_validate(const char *prefix, struct dirent *dirent)
{
	assert(prefix);

	/* Valid environment files always end in '.env' */
	if (!str_endswith(dirent->d_name, ".env")) {
		return NULL;
	}

	char *full_path = strdup_printf("%s/%s", prefix, dirent->d_name);
	if (!full_path) {
		return NULL;
	}

	/* Valid environment files must be regular files */
	struct stat statbuf;
	if (stat(full_path, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
		return full_path;
	}

	free(full_path);
	return NULL;
}

static bool
read_environment_dir(const char *path_prefix)
{
	bool success = false;
	char *path = strdup_printf("%s.d", path_prefix);

	errno = 0;
	DIR *envdir = opendir(path);

	if (!envdir) {
		if (errno != ENOENT) {
			const char *err_msg = strerror(errno);
			wlr_log(WLR_INFO,
				"failed to read environment directory: %s",
				err_msg ? err_msg : "reason unknown");
		}

		goto env_dir_cleanup;
	}

	struct dirent *dirent;
	while ((dirent = readdir(envdir)) != NULL) {
		char *env_file_path = strdup_env_path_validate(path, dirent);
		if (!env_file_path) {
			continue;
		}

		if (read_environment_file(env_file_path)) {
			success = true;
		}

		free(env_file_path);
	}

	closedir(envdir);

env_dir_cleanup:
	free(path);
	return success;
}

static void
backend_check_drm(struct wlr_backend *backend, void *is_drm)
{
	if (wlr_backend_is_drm(backend)) {
		*(bool *)is_drm = true;
	}
}

static bool
should_update_activation(struct server *server)
{
	assert(server);

	static const char *act_env = "LABWC_UPDATE_ACTIVATION_ENV";
	char *env = getenv(act_env);
	if (env) {
		/* Respect any valid preference from the environment */
		int enabled = parse_bool(env, -1);

		if (enabled == -1) {
			wlr_log(WLR_ERROR, "ignoring non-Boolean variable %s", act_env);
		} else {
			wlr_log(WLR_DEBUG, "%s is %s",
				act_env, enabled ? "true" : "false");
			return enabled;
		}
	}

	/* With no valid preference, update when a DRM backend is in use */
	bool have_drm = false;
	wlr_multi_for_each_backend(server->backend, backend_check_drm, &have_drm);
	return have_drm;
}

static void
update_activation_env(struct server *server, bool initialize)
{
	if (!should_update_activation(server)) {
		return;
	}

	if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
		/* Prevent accidentally auto-launching a dbus session */
		wlr_log(WLR_INFO, "Not updating dbus execution environment: "
			"DBUS_SESSION_BUS_ADDRESS not set");
		return;
	}

	wlr_log(WLR_INFO, "Updating dbus execution environment");

	char *env_keys = str_join(env_vars, "%s", " ");
	char *env_unset_keys = initialize ? NULL : str_join(env_vars, "%s=", " ");

	char *cmd =
		strdup_printf("dbus-update-activation-environment %s",
			initialize ? env_keys : env_unset_keys);
	spawn_async_no_shell(cmd);
	free(cmd);

	cmd = strdup_printf("systemctl --user %s %s",
		initialize ? "import-environment" : "unset-environment", env_keys);
	spawn_async_no_shell(cmd);
	free(cmd);

	free(env_keys);
	free(env_unset_keys);
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

		/* Process an environment file itself */
		bool success = read_environment_file(path->string);

		/* Process a correponding environment.d directory */
		success |= read_environment_dir(path->string);

		if (success && !should_merge_config) {
			break;
		}
	}
	paths_destroy(&paths);
}

static void
run_session_script(const char *script)
{
	struct wl_list paths;
	paths_config_create(&paths, script);

	bool should_merge_config = rc.merge_config;
	struct wl_list *(*iter)(struct wl_list *list);
	iter = should_merge_config ? paths_get_prev : paths_get_next;

	for (struct wl_list *elm = iter(&paths); elm != &paths; elm = iter(elm)) {
		struct path *path = wl_container_of(elm, path, link);
		if (!file_exists(path->string)) {
			continue;
		}
		wlr_log(WLR_INFO, "run session script %s", path->string);
		char *cmd = strdup_printf("sh %s", path->string);
		spawn_async_no_shell(cmd);
		free(cmd);

		if (!should_merge_config) {
			break;
		}
	}
	paths_destroy(&paths);
}

void
session_autostart_init(struct server *server)
{
	/* Update dbus and systemd user environment, each may fail gracefully */
	update_activation_env(server, /* initialize */ true);
	run_session_script("autostart");
}

void
session_shutdown(struct server *server)
{
	run_session_script("shutdown");

	/* Clear the dbus and systemd user environment, each may fail gracefully */
	update_activation_env(server, /* initialize */ false);
}
