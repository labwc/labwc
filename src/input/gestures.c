// SPDX-License-Identifier: GPL-2.0-only
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include "input/gestures.h"
#include "labwc.h"

static void
handle_pointer_pinch_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_begin);
	struct wlr_pointer_pinch_begin_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void
handle_pointer_pinch_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_update);
	struct wlr_pointer_pinch_update_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

static void
handle_pointer_pinch_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_end);
	struct wlr_pointer_pinch_end_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

static void
handle_pointer_swipe_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_begin);
	struct wlr_pointer_swipe_begin_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void
handle_pointer_swipe_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_update);
	struct wlr_pointer_swipe_update_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy);
}

static void
handle_pointer_swipe_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_end);
	struct wlr_pointer_swipe_end_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

void
gestures_init(struct seat *seat)
{
	seat->pointer_gestures = wlr_pointer_gestures_v1_create(seat->server->wl_display);

	seat->pinch_begin.notify = handle_pointer_pinch_begin;
	wl_signal_add(&seat->cursor->events.pinch_begin, &seat->pinch_begin);

	seat->pinch_update.notify = handle_pointer_pinch_update;
	wl_signal_add(&seat->cursor->events.pinch_update, &seat->pinch_update);

	seat->pinch_end.notify = handle_pointer_pinch_end;
	wl_signal_add(&seat->cursor->events.pinch_end, &seat->pinch_end);

	seat->swipe_begin.notify = handle_pointer_swipe_begin;
	wl_signal_add(&seat->cursor->events.swipe_begin, &seat->swipe_begin);

	seat->swipe_update.notify = handle_pointer_swipe_update;
	wl_signal_add(&seat->cursor->events.swipe_update, &seat->swipe_update);

	seat->swipe_end.notify = handle_pointer_swipe_end;
	wl_signal_add(&seat->cursor->events.swipe_end, &seat->swipe_end);
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
}
