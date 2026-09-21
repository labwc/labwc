// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <cmocka.h>
#include "config/daemonize.h"
#include "common/file-helpers.h"

static char test_path[256];

static void
set_test_path(const char *suffix)
{
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
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
