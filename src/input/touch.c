// SPDX-License-Identifier: GPL-2.0-only
#include <wayland-util.h>
#include <wlr/types/wlr_touch.h>
#include <linux/input-event-codes.h>
#include "common/macros.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "idle.h"
#include "input/touch.h"
#include "labwc.h"
#include "config/mousebind.h"
#include "action.h"

/* Holds layout -> surface offsets to report motion events in relative coords */
struct touch_point {
	int32_t touch_id;
	uint32_t x_offset;
	uint32_t y_offset;
	struct wlr_surface *surface;
	struct wl_list link; /* seat.touch_points */
};

static struct wlr_surface*
touch_get_coords(struct seat *seat, struct wlr_touch *touch, double x, double y,
		double *x_offset, double *y_offset)
{
	/*
	 * Do not return a surface when mouse emulation is enforced. Not
	 * having a surface will trigger the fallback to cursor move/button
	 * emulation in the touch signal handlers.
	 */
	struct touch_config_entry *config_entry =
		touch_find_config_for_device(touch->base.name);
	if (config_entry && config_entry->force_mouse_emulation) {
		return NULL;
	}

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
handle_touch_motion(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_motion);
	struct wlr_touch_motion_event *event = data;
	idle_manager_notify_activity(seat->seat);

	/* Find existing touch point to determine initial offsets to subtract */
	struct touch_point *touch_point;
	wl_list_for_each(touch_point, &seat->touch_points, link) {
		if (touch_point->touch_id == event->touch_id) {
			if (touch_point->surface) {
				/* Convert coordinates: first [0, 1] => layout */
				double lx, ly;
				wlr_cursor_absolute_to_layout_coords(seat->cursor,
					&event->touch->base, event->x, event->y, &lx, &ly);

				/* Apply offsets to get surface coords before reporting event */
				double sx = lx - touch_point->x_offset;
				double sy = ly - touch_point->y_offset;

				wlr_seat_touch_notify_motion(seat->seat, event->time_msec,
					event->touch_id, sx, sy);
			} else {
				cursor_emulate_move_absolute(seat, &event->touch->base,
					event->x, event->y, event->time_msec);
			}
			return;
		}
	}
}

static void
handle_touch_frame(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_frame);

	wlr_seat_touch_notify_frame(seat->seat);
}

static void
handle_touch_down(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_down);
	struct wlr_touch_down_event *event = data;

	/* Compute layout => surface offset and save for this touch point */
	struct touch_point *touch_point = znew(*touch_point);
	double x_offset = 0.0, y_offset = 0.0;
	touch_point->surface = touch_get_coords(seat, event->touch,
			event->x, event->y, &x_offset, &y_offset);
	touch_point->touch_id = event->touch_id;
	touch_point->x_offset = x_offset;
	touch_point->y_offset = y_offset;

	wl_list_insert(&seat->touch_points, &touch_point->link);

	if (touch_point->surface) {
		/* Convert coordinates: first [0, 1] => layout */
		double lx, ly;
		wlr_cursor_absolute_to_layout_coords(seat->cursor,
			&event->touch->base, event->x, event->y, &lx, &ly);

		/* Apply offsets to get surface coords before reporting event */
		double sx = lx - x_offset;
		double sy = ly - y_offset;

		struct view *view = view_from_wlr_surface(touch_point->surface);
		struct mousebind *mousebind;
		wl_list_for_each(mousebind, &rc.mousebinds, link) {
			if (mousebind->mouse_event == MOUSE_ACTION_PRESS
					&& mousebind->button == BTN_LEFT
					&& mousebind->context == LAB_SSD_CLIENT) {
				actions_run(view, seat->server, &mousebind->actions, NULL);
			}
		}

		wlr_seat_touch_notify_down(seat->seat, touch_point->surface,
			event->time_msec, event->touch_id, sx, sy);
	} else {
		cursor_emulate_move_absolute(seat, &event->touch->base,
			event->x, event->y, event->time_msec);
		cursor_emulate_button(seat, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED,
			event->time_msec);
	}
}

static void
handle_touch_up(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, touch_up);
	struct wlr_touch_up_event *event = data;

	/* Remove the touch point from the seat */
	struct touch_point *touch_point, *tmp;
	wl_list_for_each_safe(touch_point, tmp, &seat->touch_points, link) {
		if (touch_point->touch_id == event->touch_id) {
			if (touch_point->surface) {
				wlr_seat_touch_notify_up(seat->seat, event->time_msec,
					event->touch_id);
			} else {
				cursor_emulate_button(seat, BTN_LEFT,
					WL_POINTER_BUTTON_STATE_RELEASED, event->time_msec);
			}
			wl_list_remove(&touch_point->link);
			zfree(touch_point);
			break;
		}
	}
}

void
touch_init(struct seat *seat)
{
	CONNECT_SIGNAL(seat->cursor, seat, touch_down);
	CONNECT_SIGNAL(seat->cursor, seat, touch_up);
	CONNECT_SIGNAL(seat->cursor, seat, touch_motion);
	CONNECT_SIGNAL(seat->cursor, seat, touch_frame);
}

void
touch_finish(struct seat *seat)
{
	wl_list_remove(&seat->touch_down.link);
	wl_list_remove(&seat->touch_up.link);
	wl_list_remove(&seat->touch_motion.link);
	wl_list_remove(&seat->touch_frame.link);
}
