// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include "edges.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "regions.h"
#include "resize-indicator.h"
#include "snap.h"
#include "view.h"
#include "window-rules.h"

/*
 *   pos_old  pos_cursor
 *      v         v
 *      +---------+-------------------+
 *      <-----------size_old---------->
 *
 *      return value
 *           v
 *           +----+---------+
 *           <---size_new--->
 */
static int
max_move_scale(double pos_cursor, double pos_old, double size_old,
		double size_new)
{
	double anchor_frac = (pos_cursor - pos_old) / size_old;
	int pos_new = pos_cursor - (size_new * anchor_frac);
	if (pos_new < pos_old) {
		/* Clamp by using the old offsets of the maximized window */
		pos_new = pos_old;
	}
	return pos_new;
}

void
interactive_anchor_to_cursor(struct server *server, struct wlr_box *geo)
{
	assert(server->input_mode == LAB_INPUT_STATE_MOVE);
	if (wlr_box_empty(geo)) {
		return;
	}
	/* Resize grab_box while anchoring it to grab_box.{x,y} */
	server->grab_box.x = max_move_scale(server->grab_x, server->grab_box.x,
		server->grab_box.width, geo->width);
	server->grab_box.y = max_move_scale(server->grab_y, server->grab_box.y,
		server->grab_box.height, geo->height);
	server->grab_box.width = geo->width;
	server->grab_box.height = geo->height;

	geo->x = server->grab_box.x + (server->seat.cursor->x - server->grab_x);
	geo->y = server->grab_box.y + (server->seat.cursor->y - server->grab_y);
}

void
interactive_begin(struct view *view, enum input_mode mode, uint32_t edges)
{
	/*
	 * This function sets up an interactive move or resize operation, where
	 * the compositor stops propagating pointer events to clients and
	 * instead consumes them itself, to move or resize windows.
	 */
	struct server *server = view->server;
	struct seat *seat = &server->seat;

	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		return;
	}

	/* Prevent moving/resizing fixed-position and panel-like views */
	if (window_rules_get_property(view, "fixedPosition") == LAB_PROP_TRUE
			|| view_has_strut_partial(view)) {
		return;
	}

	switch (mode) {
	case LAB_INPUT_STATE_MOVE:
		if (view->fullscreen) {
			/**
			 * We don't allow moving fullscreen windows.
			 *
			 * If you think there is a good reason to allow
			 * it, feel free to open an issue explaining
			 * your use-case.
			 */
			return;
		}

		if (view_is_floating(view)) {
			/* Store natural geometry at start of move */
			view_store_natural_geometry(view);
			view_invalidate_last_layout_geometry(view);
		}

		/* Prevent region snapping when just moving via A-Left mousebind */
		struct wlr_keyboard *keyboard = &seat->keyboard_group->keyboard;
		seat->region_prevent_snap = keyboard_any_modifiers_pressed(keyboard);

		cursor_set(seat, LAB_CURSOR_GRAB);
		break;
	case LAB_INPUT_STATE_RESIZE:
		if (view->shaded || view->fullscreen ||
				view->maximized == VIEW_AXIS_BOTH) {
			/*
			 * We don't allow resizing while shaded,
			 * fullscreen or maximized in both directions.
			 */
			return;
		}

		/*
		 * Resizing overrides any attempt to restore window
		 * geometries altered by layout changes.
		 */
		view_invalidate_last_layout_geometry(view);

		/*
		 * If tiled or maximized in only one direction, reset
		 * maximized/tiled state but keep the same geometry as
		 * the starting point for the resize.
		 */
		view_set_untiled(view);
		view_restore_to(view, view->pending);
		cursor_set(seat, cursor_get_from_edge(edges));
		break;
	default:
		/* Should not be reached */
		return;
	}

	server->input_mode = mode;
	server->grabbed_view = view;
	/* Remember view and cursor positions at start of move/resize */
	server->grab_x = seat->cursor->x;
	server->grab_y = seat->cursor->y;
	server->grab_box = view->current;
	server->resize_edges = edges;

	/*
	 * Un-tile maximized/tiled view immediately if <unSnapThreshold> is
	 * zero. Otherwise, un-tile it later in cursor motion handler.
	 * If the natural geometry is unknown (possible with xdg-shell views),
	 * then we set a size of 0x0 here and determine the correct geometry
	 * later. See do_late_positioning() in xdg.c.
	 */
	if (mode == LAB_INPUT_STATE_MOVE && !view_is_floating(view)
			&& rc.unsnap_threshold <= 0) {
		struct wlr_box natural_geo = view->natural_geometry;
		interactive_anchor_to_cursor(server, &natural_geo);
		/* Shaded clients will not process resize events until unshaded */
		view_set_shade(view, false);
		view_set_untiled(view);
		view_restore_to(view, natural_geo);
	}

	wlr_seat_pointer_notify_clear_focus(seat->seat);

	if (rc.resize_indicator) {
		resize_indicator_show(view);
	}
	if (rc.window_edge_strength) {
		edges_calculate_visibility(server, view);
	}
}

enum view_edge
edge_from_cursor(struct seat *seat, struct output **dest_output)
{
	if (!view_is_floating(seat->server->grabbed_view)) {
		return VIEW_EDGE_INVALID;
	}

	int snap_range = rc.snap_edge_range;
	if (!snap_range) {
		return VIEW_EDGE_INVALID;
	}

	struct output *output = output_nearest_to_cursor(seat->server);
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "output at cursor is unusable");
		return VIEW_EDGE_INVALID;
	}
	*dest_output = output;

	/* Translate into output local coordinates */
	double cursor_x = seat->cursor->x;
	double cursor_y = seat->cursor->y;
	wlr_output_layout_output_coords(seat->server->output_layout,
		output->wlr_output, &cursor_x, &cursor_y);

	struct wlr_box *area = &output->usable_area;
	if (cursor_x <= area->x + snap_range) {
		return VIEW_EDGE_LEFT;
	} else if (cursor_x >= area->x + area->width - snap_range) {
		return VIEW_EDGE_RIGHT;
	} else if (cursor_y <= area->y + snap_range) {
		if (rc.snap_top_maximize) {
			return VIEW_EDGE_CENTER;
		} else {
			return VIEW_EDGE_UP;
		}
	} else if (cursor_y >= area->y + area->height - snap_range) {
		return VIEW_EDGE_DOWN;
	} else {
		/* Not close to any edge */
		return VIEW_EDGE_INVALID;
	}
}

/* Returns true if view was snapped to any edge */
static bool
snap_to_edge(struct view *view)
{
	struct output *output;
	enum view_edge edge = edge_from_cursor(&view->server->seat, &output);
	if (edge == VIEW_EDGE_INVALID) {
		return false;
	}

	view_set_output(view, output);
	/*
	 * Don't store natural geometry here (it was
	 * stored already in interactive_begin())
	 */
	if (edge == VIEW_EDGE_CENTER) {
		/* <topMaximize> */
		view_maximize(view, VIEW_AXIS_BOTH,
			/*store_natural_geometry*/ false);
	} else {
		view_snap_to_edge(view, edge,
			/*across_outputs*/ false,
			/*store_natural_geometry*/ false);
	}

	return true;
}

static bool
snap_to_region(struct view *view)
{
	if (!regions_should_snap(view->server)) {
		return false;
	}

	struct region *region = regions_from_cursor(view->server);
	if (region) {
		view_snap_to_region(view, region,
			/*store_natural_geometry*/ false);
		return true;
	}
	return false;
}

void
interactive_finish(struct view *view)
{
	if (view->server->grabbed_view != view) {
		return;
	}

	if (view->server->input_mode == LAB_INPUT_STATE_MOVE) {
		if (!snap_to_region(view)) {
			snap_to_edge(view);
		}
	}

	interactive_cancel(view);
}

/*
 * Cancels interactive move/resize without changing the state of the of
 * the view in any way. This may leave the tiled state inconsistent with
 * the actual geometry of the view.
 */
void
interactive_cancel(struct view *view)
{
	if (view->server->grabbed_view != view) {
		return;
	}

	overlay_hide(&view->server->seat);

	resize_indicator_hide(view);

	view->server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
	view->server->grabbed_view = NULL;

	/* Update focus/cursor image */
	cursor_update_focus(view->server);
}
