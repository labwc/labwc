// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "config/gesturebind.h"
#include "common/mem.h"

enum lab_gesture_event
gesture_parse_event(const char *name)
{
	enum lab_gesture_event event_type = LAB_GESTURE_EVENT_NONE;

	if (!strcasecmp(name, "swipe-up")) {
		event_type = LAB_GESTURE_EVENT_SWIPE_UP;
	} else if (!strcasecmp(name, "swipe-down")) {
		event_type = LAB_GESTURE_EVENT_SWIPE_DOWN;
	} else if (!strcasecmp(name, "swipe-left")) {
		event_type = LAB_GESTURE_EVENT_SWIPE_LEFT;
	} else if (!strcasecmp(name, "swipe-right")) {
		event_type = LAB_GESTURE_EVENT_SWIPE_RIGHT;
	} else if (!strcasecmp(name, "pinch-in")) {
		event_type = LAB_GESTURE_EVENT_PINCH_IN;
	} else if (!strcasecmp(name, "pinch-out")) {
		event_type = LAB_GESTURE_EVENT_PINCH_OUT;
	}

	return event_type;
}

void
gesturebind_destroy(struct lab_gesturebind *gesturebind)
{
	assert(wl_list_empty(&gesturebind->actions));
	zfree(gesturebind->device_name);
	zfree(gesturebind);
}
