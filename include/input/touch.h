/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TOUCH_H
#define LABWC_TOUCH_H

struct seat;

void touch_init(struct seat *seat);
void touch_finish(struct seat *seat);

#endif /* LABWC_TOUCH_H */
