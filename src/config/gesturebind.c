// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "config/gesturebind.h"
#include "common/mem.h"

enum lab_gesture_event gesture_parse_event(const char *name)
{
	if (!name) { return LAB_GESTURE_EVENT_NONE; }
	if (!strcasecmp(name, "swipe-up")) { return LAB_GESTURE_EVENT_SWIPE_UP; }
	if (!strcasecmp(name, "swipe-down")) { return LAB_GESTURE_EVENT_SWIPE_DOWN; }
	if (!strcasecmp(name, "swipe-left")) { return LAB_GESTURE_EVENT_SWIPE_LEFT; }
	if (!strcasecmp(name, "swipe-right")) { return LAB_GESTURE_EVENT_SWIPE_RIGHT; }
	if (!strcasecmp(name, "pinch-in")) { return LAB_GESTURE_EVENT_PINCH_IN; }
	if (!strcasecmp(name, "pinch-out")) { return LAB_GESTURE_EVENT_PINCH_OUT; }
	return LAB_GESTURE_EVENT_NONE;
}

enum gesture_type gesture_parse_type(const char *name)
{
	if (!name) { return GESTURE_TYPE_NONE; }
	if (!strcasecmp(name, "swipe-up")) { return GESTURE_TYPE_SWIPE; }
	if (!strcasecmp(name, "swipe-down")) { return GESTURE_TYPE_SWIPE; }
	if (!strcasecmp(name, "swipe-left")) { return GESTURE_TYPE_SWIPE; }
	if (!strcasecmp(name, "swipe-right")) { return GESTURE_TYPE_SWIPE; }
	if (!strcasecmp(name, "pinch-in")) { return GESTURE_TYPE_PINCH; }
	if (!strcasecmp(name, "pinch-out")) { return GESTURE_TYPE_PINCH; }
	return GESTURE_TYPE_NONE;
}

void gesturebind_destroy(struct lab_gesturebind *gesturebind)
{
	assert(wl_list_empty(&gesturebind->actions));

	zfree(gesturebind->device_name);
	zfree(gesturebind);
}
