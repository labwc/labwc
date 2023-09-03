// SPDX-License-Identifier: GPL-2.0-only
#include <wayland-util.h>
#include <wlr/types/wlr_touch.h>
#include "idle.h"
#include "labwc.h"
#include "common/mem.h"
#include "common/scene-helpers.h"

/* Holds layout -> surface offsets to report motion events in relative coords */
struct touch_point {
	int32_t touch_id;
	uint32_t x_offset;
	uint32_t y_offset;
	struct wl_list link; /* seat.touch_points */
};

static struct wlr_surface*
touch_get_coords(struct seat *seat, struct wlr_touch *touch, double x, double y,
		double *x_offset, double *y_offset)
{
	/* Convert coordinates: first [0, 1] => layout, then layout => surface */
	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &touch->base,
		x, y, &lx, &ly);

	double sx, sy;
	struct wlr_scene_node *node =
		wlr_scene_node_at(&seat->server->scene->tree.node, lx, ly, &sx, &sy);

	*x_offset = lx - sx;
	*y_offset = ly - sy;

	/* Find the surface and return it if it accepts touch events */
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
	idle_manager_notify_activity(seat->seat);

	/* Convert coordinates: first [0, 1] => layout, then apply offsets */
	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &event->touch->base,
		event->x, event->y, &lx, &ly);

	/* Find existing touch point to determine initial offsets to subtract */
	struct touch_point *touch_point;
	wl_list_for_each(touch_point, &seat->touch_points, link) {
		if (touch_point->touch_id == event->touch_id) {
			double sx = lx - touch_point->x_offset;
			double sy = ly - touch_point->y_offset;

			wlr_seat_touch_notify_motion(seat->seat, event->time_msec,
				event->touch_id, sx, sy);
			return;
		}
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

	/* Compute layout => surface offset and save for this touch point */
	double x_offset, y_offset;
	struct wlr_surface *surface = touch_get_coords(seat, event->touch,
			event->x, event->y, &x_offset, &y_offset);

	struct touch_point *touch_point = znew(*touch_point);
	touch_point->touch_id = event->touch_id;
	touch_point->x_offset = x_offset;
	touch_point->y_offset = y_offset;

	wl_list_insert(&seat->touch_points, &touch_point->link);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &event->touch->base,
		event->x, event->y, &lx, &ly);

	/* Apply offsets to get surface coords before reporting event */
	double sx = lx - x_offset;
	double sy = ly - y_offset;

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

	/* Remove the touch point from the seat */
	struct touch_point *touch_point, *tmp;
	wl_list_for_each_safe(touch_point, tmp, &seat->touch_points, link) {
		if (touch_point->touch_id == event->touch_id) {
			wl_list_remove(&touch_point->link);
			zfree(touch_point);
			break;
		}
	}

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
