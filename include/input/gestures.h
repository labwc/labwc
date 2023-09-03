/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_GESTURES_H
#define LABWC_GESTURES_H

struct seat;

void gestures_init(struct seat *seat);
void gestures_finish(struct seat *seat);

#endif /* LABWC_GESTURES_H */
