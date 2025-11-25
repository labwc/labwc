/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_GESTUREBIND_H
#define LABWC_GESTUREBIND_H

#include <stdbool.h>
#include <wayland-util.h>
#include "input/gestures.h"

#define MAX_FINGERS 10

enum lab_gesture_event {
	LAB_GESTURE_EVENT_NONE = 0,
	LAB_GESTURE_EVENT_SWIPE_UP,
	LAB_GESTURE_EVENT_SWIPE_DOWN,
	LAB_GESTURE_EVENT_SWIPE_LEFT,
	LAB_GESTURE_EVENT_SWIPE_RIGHT,
	LAB_GESTURE_EVENT_PINCH_IN,
	LAB_GESTURE_EVENT_PINCH_OUT,
	LAB_GESTURE_EVENT_COUNT,
};

struct lab_gesturebind {
	char *device_name;

	/* use at rc.xml*/
	enum lab_gesture_event event;
	int finger_count;

	struct wl_list actions;  /* struct action.link */
	struct wl_list link; /* struct rc.gesture_bindings */
};

enum lab_gesture_event gesture_parse_event(const char *name);

void gesturebind_destroy(struct lab_gesturebind *gesturebind);
#endif /* LABWC_GESTUREBIND_H */
