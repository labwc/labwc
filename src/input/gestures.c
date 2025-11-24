// SPDX-License-Identifier: GPL-2.0-only
#include "input/gestures.h"
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include "common/macros.h"
#include "labwc.h"
#include "idle.h"
#include "config/gesturebind.h"
#include "config/rcxml.h"
#include <strings.h>
#include "action.h"
#include <math.h>

static void trigger_gesture_action(struct seat *seat, enum lab_gesture_event event, int finger_count, char *device_name)
{
	if (wl_list_empty(&rc.gesture_bindings)) {
		return;
	}

	struct wl_list *default_device_actions = NULL;

	struct lab_gesturebind *gb;
	wl_list_for_each(gb, &rc.gesture_bindings, link) {
		if (gb->event == event && gb->finger_count == finger_count) {
			if (!strcasecmp(gb->device_name, device_name)) {
				actions_run(NULL, seat->server, &gb->actions, NULL);
				return;
			} else if (!strcasecmp(gb->device_name, "default")) {
				default_device_actions = &gb->actions;
			}
		}
	}

	// if doesn't match device_name, use default name(if default set)
	if (default_device_actions != NULL) {
		actions_run(NULL, seat->server, default_device_actions, NULL);
	}
}

// TODO: make configurable?
#define SWIPE_THRESHOLD 100.0
#define SWIPE_CONTINUROUS_THRESHOLD 200.0
#define PINCH_THRESHOLD_FACTOR 0.1 // 10%

static void gesture_tracker_begin(struct gesture_tracker *tracker, enum gesture_type type, uint8_t fingers)
{
    // state_tracker zero clear
    memset(tracker, 0, sizeof(*tracker));
    tracker->type = type;
    tracker->fingers = fingers;
    // scale = 1.0 when PINCH start
    if (type == GESTURE_TYPE_PINCH) {
        tracker->scale = 1.0;
    }
    tracker->continurous_mode = false;
    wlr_log(WLR_DEBUG, "Gesture begin: Type %d, Fingers %d", type, fingers);
}

static bool gesture_tracker_check(struct gesture_tracker *tracker, enum gesture_type type)
{
    return tracker->type == type;
}

static void gesture_tracker_update(struct gesture_tracker *tracker, double dx, double dy, double scale, double rotation)
{
	if (tracker->type == GESTURE_TYPE_HOLD) {
		return; //hold does not update
	}

	tracker->dx += dx;
	tracker->dy += dy;

	if (tracker->type == GESTURE_TYPE_PINCH) {
		tracker->scale = scale;
		tracker->rotation += rotation;
	}
}

static void gesture_tracker_cancel(struct gesture_tracker *tracker)
{
    tracker->type = GESTURE_TYPE_NONE;
    wlr_log(WLR_DEBUG, "Gesture cancelled.");
}

static void gesture_tracker_end(struct gesture_tracker *tracker)
{
    // reset state_tacker
    tracker->type = GESTURE_TYPE_NONE;
    tracker->fingers = 0;
    tracker->dx = tracker->dy = 0.0;
    tracker->scale = 1.0;
    tracker->rotation = 0.0;
    tracker->continurous_mode = false;
}

static bool gesture_binding_check(enum gesture_type gesture_type, int finger_count, char *input)
{
    if (wl_list_empty(&rc.gesture_bindings)) {
	    return false;
    }

    struct lab_gesturebind *gb;
    wl_list_for_each(gb, &rc.gesture_bindings, link) {
	    if (gb->bind_gesture_type == gesture_type && gb->finger_count == finger_count) {
		    // Check that input matches
		    // TODO: default override is not important ?
		    if (strcmp(gb->device_name, "default") == 0 ||
				    strcmp(gb->device_name, input) == 0) {
			    return true;
		    }
	    }
	    wlr_log(WLR_DEBUG, "Gesture binding check failed input: %s, gb.device_name: %s", input, gb->device_name);
    }
    return false;
}

static void gesture_binding_match_and_run_action(struct seat *seat, struct gesture_tracker *tracker, char *device_name)
{
    if (tracker->type == GESTURE_TYPE_SWIPE) {

        // detect swip threshold
        if (fabs(tracker->dx) >= SWIPE_THRESHOLD || fabs(tracker->dy) >= SWIPE_THRESHOLD) {

            enum lab_gesture_event event;

            // determine axis
            if (fabs(tracker->dx) > fabs(tracker->dy)) {
                event = tracker->dx > 0 ? LAB_GESTURE_EVENT_SWIPE_RIGHT : LAB_GESTURE_EVENT_SWIPE_LEFT;
            } else {
                event = tracker->dy > 0 ? LAB_GESTURE_EVENT_SWIPE_DOWN : LAB_GESTURE_EVENT_SWIPE_UP;
            }

            // execute action
            trigger_gesture_action(seat, event, tracker->fingers, device_name);
        }

    } else if (tracker->type == GESTURE_TYPE_PINCH) {

        // (1.0 +/- PINCH_THRESHOLD_FACTOR)
        if (tracker->scale >= (1.0 + PINCH_THRESHOLD_FACTOR) || tracker->scale <= (1.0 - PINCH_THRESHOLD_FACTOR)) {

            enum lab_gesture_event event = tracker->scale > 1.0
                ? LAB_GESTURE_EVENT_PINCH_OUT
                : LAB_GESTURE_EVENT_PINCH_IN;

            trigger_gesture_action(seat, event, tracker->fingers, device_name);
        }
    }
}

/**
 * end tracker and bind check
 */
static void gesture_tracker_end_and_match(struct seat *seat, struct gesture_tracker *tracker, char *device_name)
{
    gesture_binding_match_and_run_action(seat, tracker, device_name);

    // tracker reset
    gesture_tracker_end(tracker);
}

static void
handle_pinch_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_begin);
	struct wlr_pointer_pinch_begin_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	char *device_name = event->pointer ? event->pointer->base.name : NULL;
	if (gesture_binding_check(GESTURE_TYPE_PINCH, event->fingers, device_name)) {
		gesture_tracker_begin(&seat->gesture_state, GESTURE_TYPE_PINCH, event->fingers);
	} else {
		// ... otherwise forward to client
		wlr_pointer_gestures_v1_send_pinch_begin(seat->pointer_gestures,
				seat->seat, event->time_msec, event->fingers);
	}
}

static void
handle_pinch_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_update);
	struct wlr_pointer_pinch_update_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	if (gesture_tracker_check(&seat->gesture_state, GESTURE_TYPE_PINCH)) {
		gesture_tracker_update(&seat->gesture_state, event->dx, event->dy, event->scale, event->rotation);
	} else {
		// ... otherwise forward to client
		wlr_pointer_gestures_v1_send_pinch_update(seat->pointer_gestures,
				seat->seat, event->time_msec, event->dx, event->dy,
				event->scale, event->rotation);
	}
}

static void
handle_pinch_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_end);
	struct wlr_pointer_pinch_end_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	if (!gesture_tracker_check(&seat->gesture_state, GESTURE_TYPE_PINCH)) {
		wlr_pointer_gestures_v1_send_pinch_end(seat->pointer_gestures,
				seat->seat, event->time_msec, event->cancelled);
		return;
	}

	if (event->cancelled) {
		gesture_tracker_cancel(&seat->gesture_state);
		return;
	}

	// End gesture tracking and execute matched binding
	char *device_name = event->pointer ? event->pointer->base.name : NULL;
	gesture_tracker_end_and_match(seat, &seat->gesture_state, device_name);
}

static void
handle_swipe_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_begin);
	struct wlr_pointer_swipe_begin_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	char *device_name = event->pointer ? event->pointer->base.name : NULL;
	if (gesture_binding_check(GESTURE_TYPE_SWIPE, event->fingers, device_name)) {
		gesture_tracker_begin(&seat->gesture_state, GESTURE_TYPE_SWIPE, event->fingers);
	} else {
		wlr_pointer_gestures_v1_send_swipe_begin(seat->pointer_gestures,
				seat->seat, event->time_msec, event->fingers);
	}
}

static void
handle_swipe_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_update);
	struct wlr_pointer_swipe_update_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	if (gesture_tracker_check(&seat->gesture_state, GESTURE_TYPE_SWIPE)) {
		gesture_tracker_update(&seat->gesture_state, event->dx, event->dy, NAN, NAN);
		// swipe continurous mode
		struct gesture_tracker *tracker = &seat->gesture_state;
		if (fabs(tracker->dx) >= SWIPE_CONTINUROUS_THRESHOLD || fabs(tracker->dy) >= SWIPE_CONTINUROUS_THRESHOLD) {
			char *device_name = event->pointer ? event->pointer->base.name : NULL;
			gesture_tracker_end_and_match(seat, &seat->gesture_state, device_name);
			gesture_tracker_begin(&seat->gesture_state, GESTURE_TYPE_SWIPE, event->fingers);
			tracker->continurous_mode = true;
		}
	} else {
		wlr_pointer_gestures_v1_send_swipe_update(seat->pointer_gestures,
				seat->seat, event->time_msec, event->dx, event->dy);
	}
}

static void
handle_swipe_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_end);
	struct wlr_pointer_swipe_end_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	if (!gesture_tracker_check(&seat->gesture_state, GESTURE_TYPE_SWIPE)) {
		wlr_pointer_gestures_v1_send_swipe_end(seat->pointer_gestures,
				seat->seat, event->time_msec, event->cancelled);
		return;
	}

	if (event->cancelled) {
		gesture_tracker_cancel(&seat->gesture_state);
		return;
	}

	// End gesture tracking and execute matched binding
	struct gesture_tracker *tracker = &seat->gesture_state;
	if (!tracker->continurous_mode) {
		char *device_name = event->pointer ? event->pointer->base.name : NULL;
		gesture_tracker_end_and_match(seat, &seat->gesture_state, device_name);
	}
}

static void
handle_hold_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, hold_begin);
	struct wlr_pointer_hold_begin_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_hold_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void
handle_hold_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, hold_end);
	struct wlr_pointer_hold_end_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_hold_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

void
gestures_init(struct seat *seat)
{
	seat->pointer_gestures = wlr_pointer_gestures_v1_create(seat->server->wl_display);

	CONNECT_SIGNAL(seat->cursor, seat, pinch_begin);
	CONNECT_SIGNAL(seat->cursor, seat, pinch_update);
	CONNECT_SIGNAL(seat->cursor, seat, pinch_end);
	CONNECT_SIGNAL(seat->cursor, seat, swipe_begin);
	CONNECT_SIGNAL(seat->cursor, seat, swipe_update);
	CONNECT_SIGNAL(seat->cursor, seat, swipe_end);
	CONNECT_SIGNAL(seat->cursor, seat, hold_begin);
	CONNECT_SIGNAL(seat->cursor, seat, hold_end);
}

void
gestures_finish(struct seat *seat)
{
	wl_list_remove(&seat->pinch_begin.link);
	wl_list_remove(&seat->pinch_update.link);
	wl_list_remove(&seat->pinch_end.link);
	wl_list_remove(&seat->swipe_begin.link);
	wl_list_remove(&seat->swipe_update.link);
	wl_list_remove(&seat->swipe_end.link);
	wl_list_remove(&seat->hold_begin.link);
	wl_list_remove(&seat->hold_end.link);
}
