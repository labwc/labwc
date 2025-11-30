/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_GESTURES_H
#define LABWC_GESTURES_H

#include <stdbool.h>
#include <wayland-util.h>

struct seat;

enum gesture_type {
	GESTURE_TYPE_NONE = 1 << 0,
	GESTURE_TYPE_HOLD = 1 << 1,
	GESTURE_TYPE_PINCH = 1 << 2,
	GESTURE_TYPE_SWIPE = 1 << 3,
};

// Small helper struct to track gestures over time
struct gesture_tracker {
	enum gesture_type type;
	uint8_t fingers;
	double dx, dy;
	double scale;
	double rotation;
	bool continurous_mode;
};

void gestures_init(struct seat *seat);
void gestures_finish(struct seat *seat);

#endif /* LABWC_GESTURES_H */
