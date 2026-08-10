// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "common/nag.h"
#include <assert.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <wayland-server-core.h>
#include "common/buf.h"
#include "config/rcxml.h"
#include "common/spawn.h"
#include "labwc.h"

static struct buf log_buf = BUF_INIT;
static bool has_error = false;
static pid_t pid = 0;
static int pipe_w = -1;
static struct wl_event_source *write_notifier = NULL;
static size_t remaining = 0;

struct buf *
nag_get_buf(void)
{
	if (write_notifier) {
		/*
		 * Ensure we are not accidentally modifying
		 * the buffer while writing it to a client.
		 *
		 * This may happend when using nag_log() after
		 * nag_show() without a nag_reset() in between.
		 */
		wlr_log(WLR_ERROR, "Not writing to log buffer while sending to client");
		return NULL;
	}
	return &log_buf;
}

void
nag_set_error(enum wlr_log_importance importance)
{
	if (!has_error && importance == WLR_ERROR) {
		has_error = true;
	}
}

bool
nag_check_pid(pid_t exited_pid)
{
	if (pid && pid == exited_pid) {
		pid = 0;
		return true;
	}
	return false;
}

void
nag_reset(void)
{
	if (pid > 0) {
		kill(pid, SIGTERM);
		/* waitpid() is done in a generic SIGCHLD handler in src/server.c */
		pid = 0;
	}
	if (write_notifier) {
		wl_event_source_remove(write_notifier);
		write_notifier = NULL;
	}
	if (pipe_w != -1) {
		close(pipe_w);
		pipe_w = -1;
	}
	has_error = false;
	buf_clear(&log_buf);
}

static int
handle_writable(int fd, uint32_t mask, void *data)
{
	assert(write_notifier);
	assert(fd == pipe_w);
	assert(remaining > 0);
	assert(remaining <= (size_t)log_buf.len);

	ssize_t bytes = write(fd, log_buf.data + (log_buf.len - remaining), remaining);
	if (bytes < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to write errors to process %s", rc.error_command);
	} else {
		remaining -= bytes;
		if (remaining > 0) {
			/* Keep waiting */
			return 0;
		}
	}

	wl_event_source_remove(write_notifier);
	write_notifier = NULL;

	close(pipe_w);
	pipe_w = -1;
	return 0;
}

static void
nag_show(void)
{
	if (!has_error || log_buf.len == 0) {
		return;
	}

	pid = spawn_piped_async_no_shell(rc.error_command, &pipe_w);
	if (pid < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to launch process: %s", rc.error_command);
		return;
	}

	assert(!write_notifier);
	remaining = log_buf.len;
	write_notifier = wl_event_loop_add_fd(server.wl_event_loop,
		pipe_w, WL_EVENT_WRITABLE, handle_writable, NULL);
}

void
nag_show_callback(void *data)
{
	nag_show();
}

void
nag_finish(void)
{
	nag_reset();
	buf_reset(&log_buf);
}
