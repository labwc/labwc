// SPDX-License-Identifier: GPL-2.0-only
#include <wlr/types/wlr_touch.h>
#include "labwc.h"
#include "common/scene-helpers.h"

static struct wlr_surface*
touch_get_coords(struct seat *seat, struct wlr_touch *touch, double x, double y,
		double *sx, double *sy)
{
	/* Convert coordinates: first [0, 1] => layout, then layout => surface */
	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &touch->base,
		x, y, &lx, &ly);

	struct wlr_scene_node *node =
		wlr_scene_node_at(&seat->server->scene->node, lx, ly, sx, sy);

	/* Find the surface and return it if it accepts touch events. */
	struct wlr_surface *surface = lab_wlr_surface_from_node(node);

	if (surface && !wlr_surface_accepts_touch(seat->seat, surface)) {
		surface = NULL;
	}
	return surface;
}

static void
touch_motion(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_motion);
	struct wlr_touch_motion_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	double sx, sy;
	if (touch_get_coords(seat, event->touch, event->x, event->y, &sx, &sy)) {
		wlr_seat_touch_notify_motion(seat->seat, event->time_msec,
			event->touch_id, sx, sy);
	}
}

static void
touch_frame(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_frame);

	wlr_seat_touch_notify_frame(seat->seat);
}

static void
touch_down(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_down);
	struct wlr_touch_down_event *event = data;

	double sx, sy;
	struct wlr_surface *surface = touch_get_coords(seat, event->touch,
			event->x, event->y, &sx, &sy);
	if (surface) {
		wlr_seat_touch_notify_down(seat->seat, surface,
			event->time_msec, event->touch_id, sx, sy);
	}
}

static void
touch_up(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_up);
	struct wlr_touch_up_event *event = data;

	wlr_seat_touch_notify_up(seat->seat, event->time_msec, event->touch_id);
}

void
touch_init(struct seat *seat)
{
	seat->touch_down.notify = touch_down;
	wl_signal_add(&seat->cursor->events.touch_down, &seat->touch_down);
	seat->touch_up.notify = touch_up;
	wl_signal_add(&seat->cursor->events.touch_up, &seat->touch_up);
	seat->touch_motion.notify = touch_motion;
	wl_signal_add(&seat->cursor->events.touch_motion, &seat->touch_motion);
	seat->touch_frame.notify = touch_frame;
	wl_signal_add(&seat->cursor->events.touch_frame, &seat->touch_frame);
}

void
touch_finish(struct seat *seat)
{
	wl_list_remove(&seat->touch_down.link);
	wl_list_remove(&seat->touch_up.link);
	wl_list_remove(&seat->touch_motion.link);
	wl_list_remove(&seat->touch_frame.link);
}
