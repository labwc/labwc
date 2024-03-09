/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SPAWN_H
#define LABWC_SPAWN_H

#include <sys/types.h>

/**
 * spawn_primary_client - execute asynchronously
 * @command: command to be executed
 */
pid_t spawn_primary_client(const char *command);

/**
 * spawn_async_no_shell - execute asynchronously
 * @command: command to be executed
 */
void spawn_async_no_shell(char const *command);

/**
 * spawn_piped - execute asyncronously
 * @command: command to be executed
 * @pipe_fd: set to the read end of a pipe
 *           connected to stdout of the command
 *
 * Notes:
 * The returned pid_t has to be waited for to
 * not produce zombies and the pipe_fd has to
 * be closed. spawn_piped_close() can be used
 * to ensure both.
 */
pid_t spawn_piped(const char *command, int *pipe_fd);

/**
 * spawn_piped_close - clean up a previous
 *                     spawn_piped() process
 * @pid: will be waitpid()'d for
 * @pipe_fd: will be close()'d
 */
void spawn_piped_close(pid_t pid, int pipe_fd);

#endif /* LABWC_SPAWN_H */
