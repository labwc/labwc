// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "common/spawn.h"
#include "common/fd-util.h"

static void
reset_signals_and_limits(void)
{
	restore_nofile_limit();

	sigset_t set;
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);

	/* Restore ignored signals */
	signal(SIGPIPE, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);
}

static bool
set_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		wlr_log_errno(WLR_ERROR,
			"Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}
	flags = flags | FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) == -1) {
		wlr_log_errno(WLR_ERROR,
			"Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}
	return true;
}

void
spawn_async_no_shell(char const *command)
{
	GError *err = NULL;
	gchar **argv = NULL;

	assert(command);

	/* Use glib's shell-parse to mimic Openbox's behaviour */
	g_shell_parse_argv((gchar *)command, NULL, &argv, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
		return;
	}

	/*
	 * Avoid zombie processes by using a double-fork, whereby the
	 * grandchild becomes orphaned & the responsibility of the OS.
	 */
	pid_t child = 0, grandchild = 0;

	child = fork();
	switch (child) {
	case -1:
		wlr_log(WLR_ERROR, "unable to fork()");
		goto out;
	case 0:
		reset_signals_and_limits();

		setsid();
		grandchild = fork();
		if (grandchild == 0) {
			execvp(argv[0], argv);
			_exit(0);
		} else if (grandchild < 0) {
			wlr_log(WLR_ERROR, "unable to fork()");
		}
		_exit(0);
	default:
		break;
	}
	waitpid(child, NULL, 0);
out:
	g_strfreev(argv);
}

pid_t
spawn_primary_client(const char *command)
{
	assert(command);

	GError *err = NULL;
	gchar **argv = NULL;

	/* Use glib's shell-parse to mimic Openbox's behaviour */
	g_shell_parse_argv((gchar *)command, NULL, &argv, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
		return -1;
	}

	pid_t child = fork();
	switch (child) {
	case -1:
		wlr_log_errno(WLR_ERROR, "Failed to fork");
		g_strfreev(argv);
		return -1;
	case 0:
		/* child */
		close(STDIN_FILENO);
		reset_signals_and_limits();
		execvp(argv[0], argv);
		wlr_log_errno(WLR_ERROR, "Failed to execute primary client %s", command);
		_exit(1);
	default:
		g_strfreev(argv);
		return child;
	}
}

pid_t
spawn_piped(const char *command, int *pipe_fd)
{
	assert(command);

	int pipe_rw[2];
	if (pipe(pipe_rw) != 0) {
		wlr_log(WLR_ERROR, "unable to pipe()");
		return -1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(pipe_rw[0]);
		close(pipe_rw[1]);
		wlr_log(WLR_ERROR, "unable to fork()");
		return pid;
	}

	if (pid == 0) {
		/* child */
		reset_signals_and_limits();

		/*
		 * replace stdin and stderr with /dev/null
		 * and stdout with the write end of the pipe
		 */
		dup2(pipe_rw[1], STDOUT_FILENO);
		close(pipe_rw[0]);
		close(pipe_rw[1]);

		int dev_null = open("/dev/null", O_RDWR);
		if (dev_null < 0) {
			wlr_log_errno(WLR_ERROR, "opening /dev/null failed");
			/*
			 * Just close stdin and stderr and
			 * hope $command can deal with that.
			 */
			close(STDIN_FILENO);
			close(STDERR_FILENO);
		} else {
			dup2(dev_null, STDIN_FILENO);
			dup2(dev_null, STDERR_FILENO);
			close(dev_null);
		}

		execl("/bin/sh", "sh", "-c", command, NULL);
		/*
		 * Our stderr points to /dev/null or is closed
		 * at this point so logging is pretty useless.
		 */
		_exit(1);
	}

	/* labwc */
	close(pipe_rw[1]);

	/*
	 * Prevent leaking the read end of the pipe to further
	 * children forked during the lifetime of the descriptor.
	 */
	set_cloexec(pipe_rw[0]);

	*pipe_fd = pipe_rw[0];
	return pid;
}

void
spawn_piped_close(pid_t pid, int pipe_fd)
{
	close(pipe_fd);
	/* waitpid() is done in a generic SIGCHLD handler in src/server.c */
}
