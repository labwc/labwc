/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_NAG_H
#define LABWC_NAG_H

#include <stdbool.h>
#include <sys/wait.h>
#include <wlr/util/log.h>
#include "common/buf.h"

/* May return NULL if the buffer is currently written to a client */
struct buf *nag_get_buf(void);

void nag_set_error(enum wlr_log_importance importance);
bool nag_check_pid(pid_t exited_pid);
void nag_reset(void);
void nag_show_callback(void *data);
void nag_finish(void);

#define nag_log(verbosity, fmt, ...) \
do { \
	wlr_log(verbosity, fmt, ##__VA_ARGS__); \
	nag_set_error(verbosity); \
	if (nag_get_buf()) { \
		buf_add_fmt(nag_get_buf(), fmt "\n", ##__VA_ARGS__); \
	} \
} while (0)

#endif /* LABWC_NAG_H */
