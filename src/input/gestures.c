// SPDX-License-Identifier: GPL-2.0-only
#include "input/gestures.h"
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include "action.h"
#include "common/macros.h"
#include "config/mousebind.h"
#include "config/rcxml.h"
#include "input/gestures.h"
#include "labwc.h"
#include "idle.h"

static struct {
	double dx;
	double dy;
	uint32_t fingers;
} swipe_state;

static void
handle_pinch_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_begin);
	struct wlr_pointer_pinch_begin_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_pinch_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void
handle_pinch_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_update);
	struct wlr_pointer_pinch_update_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_pinch_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

static void
handle_pinch_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_end);
	struct wlr_pointer_pinch_end_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_pinch_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

static void
handle_swipe_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_begin);
	struct wlr_pointer_swipe_begin_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_swipe_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);

	swipe_state.dx = 0;
	swipe_state.dy = 0;
	swipe_state.fingers = event->fingers;
}

static void
handle_swipe_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_update);
	struct wlr_pointer_swipe_update_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_swipe_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy);

	if (swipe_state.fingers == event->fingers) {
		swipe_state.dx += event->dx;
		swipe_state.dy += event->dy;
	} else {
		swipe_state.dx = 0;
		swipe_state.dy = 0;
	}
}

static void
handle_swipe_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_end);
	struct wlr_pointer_swipe_end_event *event = data;

	idle_manager_notify_activity(seat->seat);
	cursor_set_visible(seat, /* visible */ true);

	wlr_pointer_gestures_v1_send_swipe_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);

	// TODO: check for !event->cancelled ?
	if (swipe_state.dx || swipe_state.dy) {
		enum direction direction;
		if (swipe_state.dx * swipe_state.dx > swipe_state.dy * swipe_state.dy) {
			direction = swipe_state.dx > 0
				? LAB_DIRECTION_RIGHT
				: LAB_DIRECTION_LEFT;
		} else {
			direction = swipe_state.dy > 0
				? LAB_DIRECTION_DOWN
				: LAB_DIRECTION_UP;
		}
		struct mousebind *mousebind;
		wl_list_for_each(mousebind, &rc.mousebinds, link) {
			if (mousebind->mouse_event != MOUSE_ACTION_SWIPE) {
				continue;
			}
			if (mousebind->fingers != swipe_state.fingers) {
				continue;
			}
			if (mousebind->direction != direction) {
				continue;
			}
			actions_run(/*view*/ NULL, seat->server,
				&mousebind->actions, /*cursor_ctx*/ NULL);
		}
	}
	swipe_state.dx = 0;
	swipe_state.dy = 0;
	swipe_state.fingers = 0;
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
