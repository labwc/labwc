/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_NAG_H
#define LABWC_NAG_H

#include <wlr/util/log.h>
#include "common/buf.h"

struct buf *nag_get_buf(void);
void nag_set_error(enum wlr_log_importance importance);
void nag_reset(void);
void nag_show_callback(void *data);

#define nag_log(verbosity, fmt, ...) \
do { \
	wlr_log(verbosity, fmt, ##__VA_ARGS__); \
	nag_set_error(verbosity); \
	buf_add_fmt(nag_get_buf(), fmt "\n", ##__VA_ARGS__); \
} while (0)

#endif /* LABWC_NAG_H */
