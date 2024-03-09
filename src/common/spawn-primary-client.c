// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copied from https://github.com/cage-kiosk/
 *
 * Copyright (c) 2018-2020 Jente Hidskes
 * Copyright (c) 2019 The Sway authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "common/spawn-primary-client.h"
#include "labwc.h"

static int
sigchld_handler(int fd, uint32_t mask, void *data)
{
	struct server *server = data;
	close(fd);
	if (mask & WL_EVENT_HANGUP) {
		wlr_log(WLR_DEBUG, "Child process closed normally");
	} else if (mask & WL_EVENT_ERROR) {
		wlr_log(WLR_DEBUG, "Connection closed by server");
	}
	wl_display_terminate(server->wl_display);
	return 0;
}

static bool
set_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		wlr_log(WLR_ERROR, "Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}
	flags = flags | FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) == -1) {
		wlr_log(WLR_ERROR, "Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}
	return true;
}

bool
spawn_primary_client(struct server *server, const char *command,
		pid_t *pid_out, struct wl_event_source **sigchld_source)
{
	GError *err = NULL;
	gchar **argv = NULL;

	assert(command);
	g_shell_parse_argv((gchar *)command, NULL, &argv, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
		return false;
	}

	int fd[2];
	if (pipe(fd)) {
		wlr_log(WLR_ERROR, "Unable to create pipe for primary client");
		return false;
	}

	pid_t pid = fork();
	if (!pid) {
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);
		close(fd[0]);
		execvp(argv[0], argv);
		wlr_log_errno(WLR_ERROR, "Failed to spawn primary client");
		_exit(1);
	} else if (pid == -1) {
		wlr_log_errno(WLR_ERROR, "Unable to fork");
		return false;
	}

	*pid_out = pid;

	if (!set_cloexec(fd[0]) || !set_cloexec(fd[1])) {
		return false;
	}
	close(fd[1]);

	struct wl_event_loop *event_loop = wl_display_get_event_loop(server->wl_display);
	uint32_t mask = WL_EVENT_HANGUP | WL_EVENT_ERROR;
	*sigchld_source = wl_event_loop_add_fd(event_loop, fd[0], mask,
		sigchld_handler, server);

	wlr_log(WLR_DEBUG, "Child process created with pid %d", pid);
	return true;
}
