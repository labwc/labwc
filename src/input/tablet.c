// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <linux/input-event-codes.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_scene.h>
#include "common/macros.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "config/mousebind.h"
#include "input/cursor.h"
#include "input/tablet.h"
#include "input/tablet-tool.h"
#include "input/tablet-pad.h"
#include "labwc.h"
#include "idle.h"
#include "action.h"

static enum motion
tool_motion_mode(enum motion motion, struct wlr_tablet_tool *tool)
{
	/*
	 * Absolute positioning doesn't make sense
	 * for tablet mouses and lenses.
	 */
	switch (tool->type) {
	case WLR_TABLET_TOOL_TYPE_MOUSE:
	case WLR_TABLET_TOOL_TYPE_LENS:
		return LAB_TABLET_MOTION_RELATIVE;
	default:
		return motion;
	}
}

static void
adjust_for_tablet_area(double tablet_width, double tablet_height,
		struct wlr_fbox box, double *x, double *y)
{
	if ((!box.x && !box.y && !box.width && !box.height)
			|| !tablet_width || !tablet_height) {
		return;
	}

	if (!box.width) {
		box.width = tablet_width - box.x;
	}
	if (!box.height) {
		box.height = tablet_height - box.y;
	}

	if (box.x + box.width <= tablet_width) {
		const double max_x = 1;
		double width_offset = max_x * box.x / tablet_width;
		*x = (*x - width_offset) * tablet_width / box.width;
	}
	if (box.y + box.height <= tablet_height) {
		const double max_y = 1;
		double height_offset = max_y * box.y / tablet_height;
		*y = (*y - height_offset) * tablet_height / box.height;
	}
}

static void
adjust_for_rotation(enum rotation rotation, double *x, double *y)
{
	double tmp;
	switch (rotation) {
	case LAB_ROTATE_NONE:
		break;
	case LAB_ROTATE_90:
		tmp = *x;
		*x = 1.0 - *y;
		*y = tmp;
		break;
	case LAB_ROTATE_180:
		*x = 1.0 - *x;
		*y = 1.0 - *y;
		break;
	case LAB_ROTATE_270:
		tmp = *x;
		*x = *y;
		*y = 1.0 - tmp;
		break;
	}
}

static void
adjust_for_rotation_relative(enum rotation rotation, double *dx, double *dy)
{
	double tmp;
	switch (rotation) {
	case LAB_ROTATE_NONE:
		break;
	case LAB_ROTATE_90:
		tmp = *dx;
		*dx = -*dy;
		*dy = tmp;
		break;
	case LAB_ROTATE_180:
		*dx = -*dx;
		*dy = -*dy;
		break;
	case LAB_ROTATE_270:
		tmp = *dx;
		*dx = *dy;
		*dy = -tmp;
		break;
	}
}

static void
adjust_for_motion_sensitivity(double motion_sensitivity, double *dx, double *dy)
{
	*dx = *dx * motion_sensitivity;
	*dy = *dy * motion_sensitivity;
}

static struct wlr_surface*
tablet_get_coords(struct drawing_tablet *tablet, double *x, double *y, double *dx, double *dy)
{
	*x = tablet->x;
	*y = tablet->y;
	*dx = tablet->dx;
	*dy = tablet->dy;
	adjust_for_rotation(rc.tablet.rotation, x, y);
	adjust_for_tablet_area(tablet->tablet->width_mm, tablet->tablet->height_mm,
		rc.tablet.box, x, y);
	adjust_for_rotation_relative(rc.tablet.rotation, dx, dy);
	adjust_for_motion_sensitivity(rc.tablet_tool.relative_motion_sensitivity, dx, dy);

	/*
	 * Do not return a surface when mouse emulation is enforced. Not
	 * having a surface or tablet tool (see handle_tablet_tool_proximity())
	 * will trigger the fallback to cursor move/button emulation in the
	 * tablet signal handlers.
	 */
	if (rc.tablet.force_mouse_emulation
			|| !tablet->tablet_v2) {
		return NULL;
	}

	/* convert coordinates: first [0, 1] => layout, then layout => surface */

	/* initialize here to avoid a maybe-uninitialized compiler warning */
	double lx = -1, ly = -1;
	switch (tablet->motion_mode) {
	case LAB_TABLET_MOTION_ABSOLUTE:
		wlr_cursor_absolute_to_layout_coords(tablet->seat->cursor,
			tablet->wlr_input_device, *x, *y, &lx, &ly);
		break;
	case LAB_TABLET_MOTION_RELATIVE:
		/*
		 * Deltas dx,dy will be directly passed into wlr_cursor_move,
		 * so we can add those directly here to determine our future
		 * position.
		 */
		lx = tablet->seat->cursor->x + *dx;
		ly = tablet->seat->cursor->y + *dy;
		break;
	}

	double sx, sy;
	struct wlr_scene_node *node =
		wlr_scene_node_at(&tablet->seat->server->scene->tree.node, lx, ly, &sx, &sy);

	/* find the surface and return it if it accepts tablet events */
	struct wlr_surface *surface = lab_wlr_surface_from_node(node);

	if (surface && !wlr_surface_accepts_tablet_v2(tablet->tablet_v2, surface)) {
		return NULL;
	}
	return surface;
}

static void
notify_motion(struct drawing_tablet *tablet, struct drawing_tablet_tool *tool,
		struct wlr_surface *surface, double x, double y, double dx, double dy,
		uint32_t time)
{
	bool enter_surface = false;
	/* Postpone proximity-in on a new surface when the tip is down */
	if (surface != tool->tool_v2->focused_surface && !tool->tool_v2->is_down) {
		enter_surface = true;
		wlr_tablet_v2_tablet_tool_notify_proximity_in(tool->tool_v2,
			tablet->tablet_v2, surface);
	}

	switch (tablet->motion_mode) {
	case LAB_TABLET_MOTION_ABSOLUTE:
		wlr_cursor_warp_absolute(tablet->seat->cursor,
			tablet->wlr_input_device, x, y);
		break;
	case LAB_TABLET_MOTION_RELATIVE:
		wlr_cursor_move(tablet->seat->cursor,
			tablet->wlr_input_device, dx, dy);
		break;
	}

	double sx, sy;
	bool notify = cursor_process_motion(tablet->seat->server, time, &sx, &sy);
	if (notify) {
		wlr_tablet_v2_tablet_tool_notify_motion(tool->tool_v2, sx, sy);
		if (enter_surface) {
			/*
			 * By re-using the existing cursor logic, we are also
			 * setting pointer focus with
			 * `wlr_seat_pointer_notify_clear_focus` when a tablet
			 * pen enters a surface. This is a good thing. A side
			 * effect is though, that a client will be notified
			 * about a pointer event (pointer enter at coordinate)
			 * and tablet events (proximity-in, move-to-coordinate)
			 * at the same time. Following tablet actions, as long
			 * as the pen stays on the surface, emit only tablet
			 * events. That said, the client still thinks that there
			 * is a pointer at the coordinate where the tablet
			 * entered the surface. This can conflict with e.g. menu
			 * or scrollbars because the client might be conflicted
			 * about which coordinates (pointer or tablet) have
			 * priority when both coordinates are on the same menu
			 * or scrollbar.
			 * It looks like that -1/-1 are valid coordinates (based
			 * solemnly on testing with several client). Notifying
			 * pointer motion to -1/-1, the pointer is sort-of out
			 * of the way and is never on the same control element
			 * with the tablet pen, so no conflicts anymore for the
			 * client.
			 * Note that Gnome/Mutter sends a pointer-leave
			 * notification on tablet proximity-in to the client to
			 * avoid this conflict. Going that way would probably
			 * involve much more refactoring in labwc, and I'm not
			 * sure what will break since I have the feeling that a
			 * lot of internals rely on correct pointer focus.
			 */
			wlr_seat_pointer_notify_motion(tool->seat->seat, time, -1, -1);
		}
	}
}

static void
handle_tablet_tool_proximity(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_proximity_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	struct drawing_tablet_tool *tool = ev->tool->data;
	if (!tablet) {
		wlr_log(WLR_DEBUG, "tool proximity event before tablet create");
		return;
	}

	idle_manager_notify_activity(tablet->seat->seat);

	if (ev->state == WLR_TABLET_TOOL_PROXIMITY_IN) {
		tablet->motion_mode =
			tool_motion_mode(rc.tablet_tool.motion, ev->tool);
	}

	/*
	 * Reset relative coordinates, we don't want to move the
	 * cursor on proximity-in for relative positioning.
	 */
	tablet->dx = 0;
	tablet->dy = 0;

	tablet->x = ev->x;
	tablet->y = ev->y;

	double x, y, dx, dy;
	struct wlr_surface *surface = tablet_get_coords(tablet, &x, &y, &dx, &dy);

	/*
	 * Do not attempt to create a tablet tool when mouse emulation is
	 * enforced. Not having a tool or tablet capable surface will trigger
	 * the fallback to cursor move/button emulation in the tablet signal
	 * handlers.
	 */
	if (!rc.tablet.force_mouse_emulation
			&& tablet->seat->server->tablet_manager && !tool) {
		/*
		 * Unfortunately `wlr_tool` is only present in the events, so
		 * use proximity for creating a `wlr_tablet_v2_tablet_tool`.
		 */
		tablet_tool_create(tablet->seat, ev->tool);
	}

	/*
	 * We have a tablet tool (aka pen/stylus) and a tablet protocol capable
	 * surface, let's send tablet notifications.
	 */
	if (tool && surface) {
		if (tool->tool_v2 && ev->state == WLR_TABLET_TOOL_PROXIMITY_IN) {
			notify_motion(tablet, tool, surface, x, y, dx, dy, ev->time_msec);
		}
		if (tool->tool_v2 && ev->state == WLR_TABLET_TOOL_PROXIMITY_OUT) {
			wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tool_v2);
		}
	}
}

static bool is_down_mouse_emulation = false;

static void
handle_tablet_tool_axis(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_axis_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	struct drawing_tablet_tool *tool = ev->tool->data;
	if (!tablet) {
		wlr_log(WLR_DEBUG, "tool axis event before tablet create");
		return;
	}

	idle_manager_notify_activity(tablet->seat->seat);

	/*
	 * Reset relative coordinates. If those axes aren't updated,
	 * the delta is zero.
	 */
	tablet->dx = 0;
	tablet->dy = 0;
	tablet->tilt_x = 0;
	tablet->tilt_y = 0;

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_X) {
		tablet->x = ev->x;
		tablet->dx = ev->dx;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_Y) {
		tablet->y = ev->y;
		tablet->dy = ev->dy;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) {
		tablet->distance = ev->distance;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) {
		tablet->pressure = ev->pressure;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_X) {
		tablet->tilt_x = ev->tilt_x;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_Y) {
		tablet->tilt_y = ev->tilt_y;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) {
		tablet->rotation = ev->rotation;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) {
		tablet->slider = ev->slider;
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) {
		tablet->wheel_delta = ev->wheel_delta;
	}

	double x, y, dx, dy;
	struct wlr_surface *surface = tablet_get_coords(tablet, &x, &y, &dx, &dy);

	/*
	 * We are sending tablet notifications on the following conditions:
	 * - a tablet tool (aka pen/stylus) had been created earlier on
	 *   proximity-in
	 * - there is no current tip or button press (e.g. from out-of-surface
	 *   scrolling) that started on a non tablet capable surface
	 * - the surface below the tip understands the tablet protocol and is in
	 *   pass through state (notifications are allowed to the client), or we
	 *   don't have a tablet-capable surface but are still having an active
	 *   grab (e.g. from out-of-surface scrolling).
	 */
	if (tool && !is_down_mouse_emulation && ((surface
			&& tablet->seat->server->input_mode == LAB_INPUT_STATE_PASSTHROUGH)
			|| wlr_tablet_tool_v2_has_implicit_grab(tool->tool_v2))) {
		/* motion seems to be supported by all tools */
		notify_motion(tablet, tool, surface, x, y, dx, dy, ev->time_msec);

		/* notify about other axis based on tool capabilities */
		if (ev->tool->distance) {
			wlr_tablet_v2_tablet_tool_notify_distance(tool->tool_v2,
				tablet->distance);
		}
		if (ev->tool->pressure) {
			wlr_tablet_v2_tablet_tool_notify_pressure(tool->tool_v2,
				tablet->pressure);
		}
		if (ev->tool->tilt) {
			/*
			 * From https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/stable/tablet/tablet-v2.xml
			 * "Other extra axes are in physical units as specified in the protocol.
			 * The current extra axes with physical units are tilt, rotation and
			 * wheel rotation.
			 * Sent whenever one or both of the tilt axes on a tool change. Each tilt
			 * value is in degrees, relative to the z-axis of the tablet.
			 * The angle is positive when the top of a tool tilts along the
			 * positive x or y axis."
			 * Based on that we only need to apply rotation but no area transformation.
			 */
			double tilt_x = tablet->tilt_x;
			double tilt_y = tablet->tilt_y;
			adjust_for_rotation_relative(rc.tablet.rotation, &tilt_x, &tilt_y);

			wlr_tablet_v2_tablet_tool_notify_tilt(tool->tool_v2,
				tilt_x, tilt_y);
		}
		if (ev->tool->rotation) {
			wlr_tablet_v2_tablet_tool_notify_rotation(tool->tool_v2,
				tablet->rotation);
		}
		if (ev->tool->slider) {
			wlr_tablet_v2_tablet_tool_notify_slider(tool->tool_v2,
				tablet->slider);
		}
		if (ev->tool->wheel) {
			wlr_tablet_v2_tablet_tool_notify_wheel(tool->tool_v2,
				tablet->wheel_delta, 0);
		}
	} else {
		if (ev->updated_axes & (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) {
			if (tool && tool->tool_v2->focused_surface) {
				wlr_tablet_v2_tablet_tool_notify_proximity_out(
					tool->tool_v2);
			}

			switch (tablet->motion_mode) {
			case LAB_TABLET_MOTION_ABSOLUTE:
				cursor_emulate_move_absolute(tablet->seat,
					&ev->tablet->base,
					x, y, ev->time_msec);
				break;
			case LAB_TABLET_MOTION_RELATIVE:
				cursor_emulate_move(tablet->seat,
					&ev->tablet->base,
					dx, dy, ev->time_msec);
				break;
			}
		}
	}
}

static uint32_t
to_stylus_button(uint32_t button)
{
	/*
	 * Use the mapping that is used by XWayland and GTK, even
	 * if it isn't the order one would expect. This is also
	 * consistent with the mapping for mouse emulation
	 */
	switch (button) {
	case BTN_LEFT:
		return BTN_TOOL_PEN;
	case BTN_RIGHT:
		return BTN_STYLUS2;
	case BTN_MIDDLE:
		return BTN_STYLUS;
	case BTN_SIDE:
		return BTN_STYLUS3;
	default:
		return 0;
	}
}

static void
seat_pointer_end_grab(struct drawing_tablet_tool *tool,
		struct wlr_surface *surface)
{
	if (!surface || !wlr_seat_pointer_has_grab(tool->seat->seat)) {
		return;
	}

	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_try_from_wlr_surface(surface);
	if (!xdg_surface || xdg_surface->role != WLR_XDG_SURFACE_ROLE_POPUP) {
		/*
		 * If we have an active popup grab (an open popup) and we are
		 * not on the popup itself, end that grab to close the popup.
		 * Contrary to pointer button notifications, a tablet button
		 * notification sometimes doesn't end grabs automatically on
		 * button notifications in another client (observed in GTK4),
		 * so end the grab manually.
		 */
		wlr_seat_pointer_end_grab(tool->seat->seat);
	}
}

static void
handle_tablet_tool_tip(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_tip_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	struct drawing_tablet_tool *tool = ev->tool->data;
	if (!tablet) {
		wlr_log(WLR_DEBUG, "tool tip event before tablet create");
		return;
	}

	idle_manager_notify_activity(tablet->seat->seat);

	double x, y, dx, dy;
	struct wlr_surface *surface = tablet_get_coords(tablet, &x, &y, &dx, &dy);

	uint32_t button = tablet_get_mapped_button(BTN_TOOL_PEN);

	/*
	 * We are sending tablet notifications on the following conditions:
	 * - a tablet tool (aka pen/stylus) had been created earlier on
	 *   proximity-in
	 * - there is no current tip or button press (e.g. from out-of-surface
	 *   scrolling) that started on a non tablet capable surface
	 * - the surface below tip understands the tablet protocol, or we don't
	 *   have a tablet-capable surface but are still having an active grab.
	 */
	if (tool && !is_down_mouse_emulation && (surface
			|| wlr_tablet_tool_v2_has_implicit_grab(tool->tool_v2))) {
		uint32_t stylus_button = to_stylus_button(button);
		if (stylus_button != BTN_TOOL_PEN) {
			wlr_log(WLR_INFO, "ignoring stylus tool pen mapping for tablet mode");
		}

		if (ev->state == WLR_TABLET_TOOL_TIP_DOWN) {
			bool notify = cursor_process_button_press(tool->seat, BTN_LEFT,
				ev->time_msec);
			if (notify) {
				seat_pointer_end_grab(tool, surface);
				wlr_tablet_v2_tablet_tool_notify_down(tool->tool_v2);
				wlr_tablet_tool_v2_start_implicit_grab(tool->tool_v2);
			}
		} else if (ev->state == WLR_TABLET_TOOL_TIP_UP) {
			bool notify = cursor_process_button_release(tool->seat, BTN_LEFT,
				ev->time_msec);
			if (notify) {
				wlr_tablet_v2_tablet_tool_notify_up(tool->tool_v2);
			}

			bool exit_interactive = cursor_finish_button_release(tool->seat, BTN_LEFT);
			if (exit_interactive && surface && tool->tool_v2->focused_surface) {
				/*
				 * Re-enter the surface after a resize/move to ensure
				 * being back in tablet mode, but only if we are still
				 * above a tablet capable surface.
				 */
				wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tool_v2);
				wlr_tablet_v2_tablet_tool_notify_proximity_in(tool->tool_v2,
					tablet->tablet_v2, surface);
			}

			if (!surface) {
				/* Out-of-surface movement ended on the desktop */
				wlr_tablet_v2_tablet_tool_notify_proximity_out(tool->tool_v2);
			}
		}
	} else {
		if (button) {
			is_down_mouse_emulation = ev->state == WLR_TABLET_TOOL_TIP_DOWN;
			cursor_emulate_button(tablet->seat,
				button,
				ev->state == WLR_TABLET_TOOL_TIP_DOWN
					? WL_POINTER_BUTTON_STATE_PRESSED
					: WL_POINTER_BUTTON_STATE_RELEASED,
				ev->time_msec);
		}
	}
}

static void
handle_tablet_tool_button(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_button_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	struct drawing_tablet_tool *tool = ev->tool->data;
	if (!tablet) {
		wlr_log(WLR_DEBUG, "tool button event before tablet create");
		return;
	}

	idle_manager_notify_activity(tablet->seat->seat);

	double x, y, dx, dy;
	struct wlr_surface *surface = tablet_get_coords(tablet, &x, &y, &dx, &dy);

	uint32_t button = tablet_get_mapped_button(ev->button);

	/*
	 * We are sending tablet notifications on the following conditions:
	 * - a tablet tool (aka pen/stylus) had been created earlier on
	 *   proximity-in
	 * - there is no current tip or button press (e.g. out of surface
	 *   scrolling) that started on a non tablet capable surface
	 * - the surface below the tip understands the tablet protocol.
	 */
	if (tool && !is_down_mouse_emulation && surface) {
		if (button && ev->state == WLR_BUTTON_PRESSED) {
			struct view *view = view_from_wlr_surface(surface);
			struct mousebind *mousebind;
			wl_list_for_each(mousebind, &rc.mousebinds, link) {
				if (mousebind->mouse_event == MOUSE_ACTION_PRESS
						&& mousebind->button == button
						&& mousebind->context == LAB_SSD_CLIENT) {
					actions_run(view, tool->seat->server,
						&mousebind->actions, NULL);
				}
			}
		}

		uint32_t stylus_button = to_stylus_button(button);
		if (stylus_button && stylus_button != BTN_TOOL_PEN) {
			if (ev->state == WLR_BUTTON_PRESSED) {
				seat_pointer_end_grab(tool, surface);
			}
			wlr_tablet_v2_tablet_tool_notify_button(tool->tool_v2,
				stylus_button,
				ev->state == WLR_BUTTON_PRESSED
					? ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED
					: ZWP_TABLET_PAD_V2_BUTTON_STATE_RELEASED);
		} else {
			wlr_log(WLR_INFO, "invalid stylus button mapping for tablet mode");
		}
	} else {
		if (button) {
			is_down_mouse_emulation = ev->state == WLR_BUTTON_PRESSED;
			cursor_emulate_button(tablet->seat, button,
				ev->state == WLR_BUTTON_PRESSED
					? WL_POINTER_BUTTON_STATE_PRESSED
					: WL_POINTER_BUTTON_STATE_RELEASED,
				ev->time_msec);
		}
	}
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet *tablet =
		wl_container_of(listener, tablet, handlers.destroy);

	wl_list_remove(&tablet->link);
	tablet_pad_attach_tablet(tablet->seat);

	wl_list_remove(&tablet->handlers.destroy.link);
	free(tablet);
}

void
tablet_create(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_DEBUG, "setting up tablet");
	struct drawing_tablet *tablet = znew(*tablet);
	tablet->seat = seat;
	tablet->wlr_input_device = wlr_device;
	tablet->tablet = wlr_tablet_from_input_device(wlr_device);
	tablet->tablet->data = tablet;
	if (seat->server->tablet_manager) {
		tablet->tablet_v2 = wlr_tablet_create(
			seat->server->tablet_manager, seat->seat, wlr_device);
	}
	tablet->x = 0.0;
	tablet->y = 0.0;
	tablet->distance = 0.0;
	tablet->pressure = 0.0;
	tablet->tilt_x = 0.0;
	tablet->tilt_y = 0.0;
	tablet->rotation = 0.0;
	tablet->slider = 0.0;
	tablet->wheel_delta = 0.0;
	wlr_log(WLR_INFO, "tablet dimensions: %.2fmm x %.2fmm",
		tablet->tablet->width_mm, tablet->tablet->height_mm);
	CONNECT_SIGNAL(wlr_device, &tablet->handlers, destroy);

	wl_list_insert(&seat->tablets, &tablet->link);
	tablet_pad_attach_tablet(tablet->seat);
}

void
tablet_init(struct seat *seat)
{
	CONNECT_SIGNAL(seat->cursor, seat, tablet_tool_axis);
	CONNECT_SIGNAL(seat->cursor, seat, tablet_tool_proximity);
	CONNECT_SIGNAL(seat->cursor, seat, tablet_tool_tip);
	CONNECT_SIGNAL(seat->cursor, seat, tablet_tool_button);
}

void
tablet_finish(struct seat *seat)
{
	wl_list_remove(&seat->tablet_tool_axis.link);
	wl_list_remove(&seat->tablet_tool_proximity.link);
	wl_list_remove(&seat->tablet_tool_tip.link);
	wl_list_remove(&seat->tablet_tool_button.link);
}
