/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_LOG_H
#define LABWC_LOG_H

#include <wlr/util/log.h>
#include "common/buf.h"

struct buf *log_get_buf(void);
void log_set_error(enum wlr_log_importance importance);
void log_reset(void);
void nag_show(void);
void nag_show_callback(void *data);

#define nag_log(verb, fmt, ...) \
do { \
	wlr_log(verb, fmt, ##__VA_ARGS__);						\
	log_set_error(verb);									\
	buf_add_fmt(log_get_buf(), fmt "\n", ##__VA_ARGS__);	\
} while (0)

#endif /* LABWC_LOG_H */
