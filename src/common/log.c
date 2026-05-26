// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "common/log.h"
#include "common/spawn.h"
#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common/buf.h"
#include "config/rcxml.h"

static struct buf log_buf = BUF_INIT;
static bool has_error = false;
static pid_t pid = 0;

struct buf *
log_get_buf(void)
{
	return &log_buf;
}

void
log_set_error(enum wlr_log_importance importance)
{
	if (!has_error && importance == WLR_ERROR) {
		has_error = true;
	}
}

void
log_reset(void)
{
	has_error = false;
	buf_reset(&log_buf);
	if (pid > 0) {
		if (!waitpid(pid, NULL, WNOHANG)) {
			kill(pid, SIGTERM);
			/* waitpid() is done in a generic SIGCHLD handler in src/server.c */
		}
	}
	pid = 0;
}

void
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
