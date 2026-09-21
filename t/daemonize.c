// SPDX-License-Identifier: GPL-2.0-only
#define _DEFAULT_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cmocka.h>
#include "config/daemonize.h"
#include "common/file-helpers.h"

static char test_path[256];
static char original_home[256];
static char scratch_home[256];

static void
remove_tree(const char *path)
{
	DIR *directory = opendir(path);
	if (directory) {
		struct dirent *entry;
		while ((entry = readdir(directory))) {
			if (!strcmp(entry->d_name, ".")
					|| !strcmp(entry->d_name, "..")) {
				continue;
			}
			char child[512];
			snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
			remove_tree(child);
		}
		closedir(directory);
	}
	remove(path);
}

static void
ensure_user_unit_dir(void)
{
	const char *home = getenv("HOME");
	if (!home || !*home) {
		return;
	}
	char path[512];
	snprintf(path, sizeof(path), "%s/.config", home);
	mkdir(path, 0700);
	snprintf(path, sizeof(path), "%s/.config/systemd", home);
	mkdir(path, 0700);
	snprintf(path, sizeof(path), "%s/.config/systemd/user", home);
	mkdir(path, 0700);
}

/*
 * These tests write into $HOME/.config/systemd/user/. Point HOME at a
 * scratch directory so that running the suite can never overwrite or
 * delete the real user configuration, including the units of a running
 * labwc session.
 */
static int
setup(void **state)
{
	(void)state;
	const char *home = getenv("HOME");
	snprintf(original_home, sizeof(original_home), "%s", home ? home : "");

	char template[] = "/tmp/labwc-daemonize-test-XXXXXX";
	char *directory = mkdtemp(template);
	if (!directory) {
		return -1;
	}
	snprintf(scratch_home, sizeof(scratch_home), "%s", directory);
	setenv("HOME", scratch_home, 1);
	ensure_user_unit_dir();
	return 0;
}

static int
teardown(void **state)
{
	(void)state;
	if (original_home[0]) {
		setenv("HOME", original_home, 1);
	}
	if (scratch_home[0]) {
		remove_tree(scratch_home);
	}
	return 0;
}

static void
set_test_path(const char *suffix)
{
	ensure_user_unit_dir();
	const char *home = getenv("HOME");
	if (!home || !*home) {
		snprintf(test_path, sizeof(test_path),
			"/tmp/labwc-daemonize-test/%s", suffix);
	} else {
		snprintf(test_path, sizeof(test_path),
			"%s/.config/systemd/user/%s", home, suffix);
	}
	daemonize_apply_path(test_path, false);
	if (file_exists(test_path)) {
		unlink(test_path);
	}
}

static void
remove_test_path(const char *path)
{
	if (file_exists(path)) {
		unlink(path);
	}
	char tmp[256];
	strncpy(tmp, path, sizeof(tmp) - 1);
	tmp[sizeof(tmp) - 1] = '\0';
	char *slash = strrchr(tmp, '/');
	if (slash) {
		*slash = '\0';
		rmdir(tmp);
		*slash = '/';
		char *parent = strrchr(tmp, '/');
		if (parent && parent > tmp) {
			*parent = '\0';
			rmdir(tmp);
			*parent = '/';
		}
	}
}

static void
test_disabled_removes_service(void **state)
{
	set_test_path("labwc.service");
	FILE *fp = fopen(test_path, "we");
	assert_non_null(fp);
	fclose(fp);

	daemonize_apply_path(test_path, false);
	assert_false(file_exists(test_path));
	remove_test_path(test_path);
}

static void
test_enabled_creates_service(void **state)
{
	set_test_path("labwc.service");
	daemonize_apply_path(test_path, true);
	assert_true(file_exists(test_path));

	char expected[] =
		"[Unit]\n"
		"Description=A Wayland window-stacking compositor\n"
		"BindsTo=graphical-session.target\n"
		"Before=graphical-session.target\n"
		"Wants=graphical-session-pre.target\n"
		"After=graphical-session-pre.target\n"
		"\n"
		"Wants=xdg-desktop-autostart.target\n"
		"Before=xdg-desktop-autostart.target\n"
		"\n"
		"[Service]\n"
		"Slice=session.slice\n"
		"Type=notify\n"
		"ExecStart=labwc\n";

	FILE *fp = fopen(test_path, "re");
	assert_non_null(fp);
	char buf[512];
	size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
	buf[n] = '\0';
	fclose(fp);
	assert_int_equal(n, sizeof(expected) - 1);
	assert_memory_equal(buf, expected, n);
	remove_test_path(test_path);
}

static void
test_enabled_is_idempotent(void **state)
{
	set_test_path("labwc.service");
	FILE *fp = fopen(test_path, "we");
	assert_non_null(fp);
	fprintf(fp, "stale\n");
	fclose(fp);

	daemonize_apply_path(test_path, true);

	char expected[] =
		"[Unit]\n"
		"Description=A Wayland window-stacking compositor\n"
		"BindsTo=graphical-session.target\n"
		"Before=graphical-session.target\n"
		"Wants=graphical-session-pre.target\n"
		"After=graphical-session-pre.target\n"
		"\n"
		"Wants=xdg-desktop-autostart.target\n"
		"Before=xdg-desktop-autostart.target\n"
		"\n"
		"[Service]\n"
		"Slice=session.slice\n"
		"Type=notify\n"
		"ExecStart=labwc\n";

	FILE *r = fopen(test_path, "re");
	assert_non_null(r);
	char buf[512];
	size_t n = fread(buf, 1, sizeof(buf) - 1, r);
	buf[n] = '\0';
	fclose(r);
	assert_int_equal(n, sizeof(expected) - 1);
	assert_memory_equal(buf, expected, n);
	remove_test_path(test_path);
}

static void
test_disabled_missing_path(void **state)
{
	set_test_path("labwc.service");
	daemonize_apply_path(test_path, false);
	assert_false(file_exists(test_path));
	remove_test_path(test_path);
}

static void
test_enabled_missing_directory(void **state)
{
	set_test_path("labwc.service");
	char path[256];
	snprintf(path, sizeof(path), "%s/missing/dir/labwc.service", test_path);
	daemonize_apply_path(path, true);
	assert_false(file_exists(path));
	remove_test_path(test_path);
}

static void
test_enabled_readonly_parent(void **state)
{
	char parent[256];
	snprintf(parent, sizeof(parent), "%s-parent", test_path);
	mkdir(parent, 0500);
	chmod(parent, 0500);

	char path[256];
	snprintf(path, sizeof(path), "%s/labwc.service", parent);

	daemonize_apply_path(path, true);

	assert_false(file_exists(path));

	chmod(parent, 0700);
	rmdir(parent);
}

static void
test_enabled_existing_correct_contents(void **state)
{
	set_test_path("labwc.service");
	const char expected[] =
		"[Unit]\n"
		"Description=A Wayland window-stacking compositor\n"
		"BindsTo=graphical-session.target\n"
		"Before=graphical-session.target\n"
		"Wants=graphical-session-pre.target\n"
		"After=graphical-session-pre.target\n"
		"\n"
		"Wants=xdg-desktop-autostart.target\n"
		"Before=xdg-desktop-autostart.target\n"
		"\n"
		"[Service]\n"
		"Slice=session.slice\n"
		"Type=notify\n"
		"ExecStart=labwc\n";

	FILE *fp = fopen(test_path, "we");
	assert_non_null(fp);
	fwrite(expected, 1, sizeof(expected) - 1, fp);
	fclose(fp);

	daemonize_apply_path(test_path, true);

	FILE *r = fopen(test_path, "re");
	assert_non_null(r);
	char buf[sizeof(expected)];
	size_t n = fread(buf, 1, sizeof(buf) - 1, r);
	buf[n] = '\0';
	fclose(r);
	assert_int_equal(n, sizeof(expected) - 1);
	assert_memory_equal(buf, expected, n);
	remove_test_path(test_path);
}

static void
test_enabled_creates_session_target(void **state)
{
	set_test_path("labwc-session.target");
	daemonize_apply_path(test_path, true);
	assert_true(file_exists(test_path));

	const char expected[] =
		"[Unit]\n"
		"Description=labwc session\n"
		"Documentation=man:labwc(1) man:systemd.special(7)\n"
		"BindsTo=graphical-session.target\n"
		"Wants=graphical-session-pre.target\n"
		"After=graphical-session-pre.target\n";

	FILE *fp = fopen(test_path, "re");
	assert_non_null(fp);
	char buf[512];
	size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
	buf[n] = '\0';
	fclose(fp);
	assert_memory_equal(buf, expected, sizeof(expected) - 1);
	remove_test_path(test_path);
}

static void
test_enabled_creates_shutdown_target(void **state)
{
	set_test_path("labwc-shutdown.target");
	daemonize_apply_path(test_path, true);
	assert_true(file_exists(test_path));

	const char expected[] =
		"[Unit]\n"
		"Description=labwc Session Shutdown\n"
		"Documentation=man:labwc(1) man:systemd.special(7)\n"
		"DefaultDependencies=no\n"
		"StopWhenUnneeded=yes\n"
		"Conflicts=graphical-session.target graphical-session-pre.target\n"
		"After=graphical-session.target graphical-session-pre.target\n";

	FILE *fp = fopen(test_path, "re");
	assert_non_null(fp);
	char buf[512];
	size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
	buf[n] = '\0';
	fclose(fp);
	assert_memory_equal(buf, expected, sizeof(expected) - 1);
	remove_test_path(test_path);
}

static void
test_disabled_removes_session_target(void **state)
{
	set_test_path("labwc-session.target");
	FILE *fp = fopen(test_path, "we");
	assert_non_null(fp);
	fclose(fp);

	daemonize_apply_path(test_path, false);
	assert_false(file_exists(test_path));
	remove_test_path(test_path);
}

static void
test_disabled_removes_shutdown_target(void **state)
{
	set_test_path("labwc-shutdown.target");
	FILE *fp = fopen(test_path, "we");
	assert_non_null(fp);
	fclose(fp);

	daemonize_apply_path(test_path, false);
	assert_false(file_exists(test_path));
	remove_test_path(test_path);
}

static void
test_apply_creates_unit_directory(void **state)
{
	(void)state;
	char path[512];
	snprintf(path, sizeof(path), "%s/.config/systemd/user", getenv("HOME"));
	rmdir(path);

	daemonize_apply(true);

	snprintf(path, sizeof(path), "%s/.config/systemd/user/labwc.service", getenv("HOME"));
	assert_true(file_exists(path));
	snprintf(path, sizeof(path), "%s/.config/systemd/user/labwc-session.target", getenv("HOME"));
	assert_true(file_exists(path));
	snprintf(path, sizeof(path), "%s/.config/systemd/user/labwc-shutdown.target", getenv("HOME"));
	assert_true(file_exists(path));
}

int
main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_disabled_removes_service),
		cmocka_unit_test(test_enabled_creates_service),
		cmocka_unit_test(test_enabled_is_idempotent),
		cmocka_unit_test(test_disabled_missing_path),
		cmocka_unit_test(test_enabled_missing_directory),
		cmocka_unit_test(test_enabled_readonly_parent),
		cmocka_unit_test(test_enabled_existing_correct_contents),
		cmocka_unit_test(test_enabled_creates_session_target),
		cmocka_unit_test(test_enabled_creates_shutdown_target),
		cmocka_unit_test(test_disabled_removes_session_target),
		cmocka_unit_test(test_disabled_removes_shutdown_target),
		cmocka_unit_test(test_apply_creates_unit_directory),
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
