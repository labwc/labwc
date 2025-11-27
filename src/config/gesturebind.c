// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "config/gesturebind.h"
#include "common/mem.h"

struct parsed_gesture
gesture_parse_event(const char *name)
{
	enum lab_gesture_event event = LAB_GESTURE_EVENT_NONE;
	enum gesture_type type = GESTURE_TYPE_NONE;

	if (!strcasecmp(name, "swipe-up")) {
		event = LAB_GESTURE_EVENT_SWIPE_UP;
		type = GESTURE_TYPE_SWIPE;
	} else if (!strcasecmp(name, "swipe-down")) {
		event = LAB_GESTURE_EVENT_SWIPE_DOWN;
		type = GESTURE_TYPE_SWIPE;
	} else if (!strcasecmp(name, "swipe-left")) {
		event = LAB_GESTURE_EVENT_SWIPE_LEFT;
		type = GESTURE_TYPE_SWIPE;
	} else if (!strcasecmp(name, "swipe-right")) {
		event = LAB_GESTURE_EVENT_SWIPE_RIGHT;
		type = GESTURE_TYPE_SWIPE;
	} else if (!strcasecmp(name, "pinch-in")) {
		event = LAB_GESTURE_EVENT_PINCH_IN;
		type = GESTURE_TYPE_PINCH;
	} else if (!strcasecmp(name, "pinch-out")) {
		event = LAB_GESTURE_EVENT_PINCH_OUT;
		type = GESTURE_TYPE_PINCH;
	}

	struct parsed_gesture gesture = { event, type };
	return gesture;
}

void
gesturebind_destroy(struct lab_gesturebind *gesturebind)
{
	assert(wl_list_empty(&gesturebind->actions));
	zfree(gesturebind->device_name);
	zfree(gesturebind);
}
