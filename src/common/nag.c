// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "common/nag.h"
#include <stdarg.h>
#include <unistd.h>
#include "common/buf.h"
#include "config/rcxml.h"
#include "common/spawn.h"

static struct buf log_buf = BUF_INIT;
static bool has_error = false;
static pid_t pid = 0;

struct buf *
nag_get_buf(void)
{
	return &log_buf;
}

void
nag_set_error(enum wlr_log_importance importance)
{
	if (!has_error && importance == WLR_ERROR) {
		has_error = true;
	}
}

void
nag_check_pid(pid_t exited_pid)
{
	if (pid && pid == exited_pid) {
		pid = 0;
	}
}

void
nag_reset(void)
{
	has_error = false;
	buf_reset(&log_buf);
	if (pid > 0) {
		kill(pid, SIGTERM);
		/* waitpid() is done in a generic SIGCHLD handler in src/server.c */
	}
	pid = 0;
}

static void
nag_show(void)
{
	if (!has_error) {
		return;
	}
	int pipe_w;
	pid = spawn_piped_async_no_shell(rc.error_command, &pipe_w);
	if (pid > 0) {
		ssize_t bytes = write(pipe_w, log_buf.data, log_buf.len);
		if (bytes < 0) {
			wlr_log(WLR_ERROR, "Failed to write errors to process: %s",
				rc.error_command);
		}
		spawn_piped_close(pid, pipe_w);
	} else if (pid < 0) {
		wlr_log(WLR_ERROR, "Failed to launch process: %s", rc.error_command);
	}
}

void
nag_show_callback(void *data)
{
	nag_show();
}
