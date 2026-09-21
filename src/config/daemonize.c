// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config/daemonize.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <wlr/util/log.h>
#include "common/file-helpers.h"

static char service_path[256];

static const char service_contents[] =
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

static const char session_target_contents[] =
	"[Unit]\n"
	"Description=labwc session\n"
	"Documentation=man:labwc(1) man:systemd.special(7)\n"
	"BindsTo=graphical-session.target\n"
	"Wants=graphical-session-pre.target\n"
	"After=graphical-session-pre.target\n";

static const char shutdown_target_contents[] =
	"[Unit]\n"
	"Description=labwc Session Shutdown\n"
	"Documentation=man:labwc(1) man:systemd.special(7)\n"
	"DefaultDependencies=no\n"
	"StopWhenUnneeded=yes\n"
	"Conflicts=graphical-session.target graphical-session-pre.target\n"
	"After=graphical-session.target graphical-session-pre.target\n";

static void
write_file_at(const char *path, const char *data, size_t len)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "failed to create %s", path);
		return;
	}
	FILE *fp = fdopen(fd, "we");
	if (!fp) {
		close(fd);
		wlr_log_errno(WLR_ERROR, "failed to open %s", path);
		return;
	}
	if (fwrite(data, 1, len, fp) != len) {
		wlr_log_errno(WLR_ERROR, "failed to write %s", path);
		fclose(fp);
		return;
	}
	fclose(fp);
	wlr_log(WLR_INFO, "created %s", path);
}

static void
apply_at(const char *path, bool enabled)
{
	if (enabled) {
		const char *data = NULL;
		size_t len = 0;
		if (strstr(path, "/labwc.service") != NULL && strstr(path, "/.config/systemd/user/") != NULL) {
			data = service_contents;
			len = sizeof(service_contents) - 1;
		} else if (strstr(path, "/labwc-session.target") != NULL) {
			data = session_target_contents;
			len = sizeof(session_target_contents) - 1;
		} else if (strstr(path, "/labwc-shutdown.target") != NULL) {
			data = shutdown_target_contents;
			len = sizeof(shutdown_target_contents) - 1;
		}
		if (data) {
			write_file_at(path, data, len);
		}
	} else if (file_exists(path)) {
		if (unlink(path) != 0) {
			wlr_log_errno(WLR_ERROR, "failed to remove %s", path);
			return;
		}
		wlr_log(WLR_INFO, "removed %s", path);
	}
}

void
daemonize_set_path(void)
{
	const char *home = getenv("HOME");
	if (!home || !*home) {
		home = "/tmp";
	}
	snprintf(service_path, sizeof(service_path),
		"%s/.config/systemd/user/labwc.service", home);
}

const char *
daemonize_get_path(void)
{
	return service_path;
}

void
daemonize_apply(bool enabled)
{
	daemonize_set_path();
	wlr_log(WLR_INFO, "daemonize path: %s", service_path);
	apply_at(service_path, enabled);
	if (enabled) {
		char target_path[256];
		snprintf(target_path, sizeof(target_path),
			"%s/.config/systemd/user/labwc-session.target", getenv("HOME") ?: "/tmp");
		apply_at(target_path, enabled);
		snprintf(target_path, sizeof(target_path),
			"%s/.config/systemd/user/labwc-shutdown.target", getenv("HOME") ?: "/tmp");
		apply_at(target_path, enabled);
		if (file_exists(service_path)) {
			wlr_log(WLR_INFO, "daemonize enabled at %s", service_path);
		} else {
			wlr_log(WLR_ERROR, "daemonize enabled but %s was not created", service_path);
		}
	} else if (!file_exists(service_path)) {
		wlr_log(WLR_INFO, "daemonize disabled, %s removed", service_path);
	} else {
		wlr_log(WLR_ERROR, "daemonize disabled but %s still exists", service_path);
	}
}

void
daemonize_notify_ready(void)
{
	const char *notify_socket = getenv("NOTIFY_SOCKET");
	const char *notify_fd = getenv("NOTIFY_FD");
	if (!notify_socket || !*notify_socket) {
		if (!notify_fd || !*notify_fd) {
			return;
		}
		int fd = atoi(notify_fd);
		if (fd <= 0) {
			return;
		}
		unsetenv("NOTIFY_FD");
		if (dprintf(fd, "READY=1") < 0) {
			wlr_log_errno(WLR_ERROR, "failed to write notify fd");
		} else {
			wlr_log(WLR_INFO, "notified systemd via fd that labwc is ready");
		}
		close(fd);
		return;
	}
	if (sd_notify(1, "READY=1") < 0) {
		wlr_log(WLR_ERROR, "failed to notify systemd");
	} else {
		wlr_log(WLR_INFO, "notified systemd that labwc is ready");
	}
}

void
daemonize_apply_path(const char *path, bool enabled)
{
	apply_at(path, enabled);
}
