/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DAEMONIZE_H
#define LABWC_DAEMONIZE_H

#include <stdbool.h>

void daemonize_apply(bool enabled);
void daemonize_apply_path(const char *path, bool enabled);
void daemonize_notify_ready(void);

#endif /* LABWC_DAEMONIZE_H */
