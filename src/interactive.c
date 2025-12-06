// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_cursor.h>
#include "config/rcxml.h"
#include "edges.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "output.h"
#include "regions.h"
#include "resize-indicator.h"
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
	/* Resize grab_box while anchoring it to grab_{x,y} */
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
interactive_begin(struct view *view, enum input_mode mode, enum lab_edge edges)
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

	enum lab_cursors cursor_shape = LAB_CURSOR_DEFAULT;

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

		/* Store natural geometry at start of move */
		view_store_natural_geometry(view);
		if (view_is_floating(view)) {
			view_invalidate_last_layout_geometry(view);
		}

		/* Prevent region snapping when just moving via A-Left mousebind */
		seat->region_prevent_snap = keyboard_get_all_modifiers(seat);

		cursor_shape = LAB_CURSOR_GRAB;
		break;
	case LAB_INPUT_STATE_RESIZE: {
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
		 * tiled state and un-maximize the relevant axes, but
		 * keep the same geometry as the starting point.
		 */
		enum view_axis maximized = view->maximized;
		if (edges & LAB_EDGES_LEFT_RIGHT) {
			maximized &= ~VIEW_AXIS_HORIZONTAL;
		}
		if (edges & LAB_EDGES_TOP_BOTTOM) {
			maximized &= ~VIEW_AXIS_VERTICAL;
		}
		view_set_maximized(view, maximized);
		view_set_untiled(view);
		cursor_shape = cursor_get_from_edge(edges);
		break;
	}
	default:
		/* Should not be reached */
		return;
	}

	server->grabbed_view = view;
	/* Remember view and cursor positions at start of move/resize */
	server->grab_x = seat->cursor->x;
	server->grab_y = seat->cursor->y;
	server->grab_box = view->current;
	server->resize_edges = edges;

	seat_focus_override_begin(seat, mode, cursor_shape);

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
		view_set_maximized(view, VIEW_AXIS_NONE);
		view_set_untiled(view);
		view_move_resize(view, natural_geo);
	}

	if (rc.resize_indicator) {
		resize_indicator_show(view);
	}
	if (rc.window_edge_strength) {
		edges_calculate_visibility(server, view);
	}
}

bool
edge_from_cursor(struct seat *seat, struct output **dest_output,
		enum lab_edge *edge1, enum lab_edge *edge2)
{
	*dest_output = NULL;
	*edge1 = LAB_EDGE_NONE;
	*edge2 = LAB_EDGE_NONE;

	if (!view_is_floating(seat->server->grabbed_view)) {
		return false;
	}

	if (rc.snap_edge_range_inner == 0 && rc.snap_edge_range_outer == 0) {
		return false;
	}

	struct output *output = output_nearest_to_cursor(seat->server);
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "output at cursor is unusable");
		return false;
	}
	*dest_output = output;

	double cursor_x = seat->cursor->x;
	double cursor_y = seat->cursor->y;

	int top_range = rc.snap_edge_range_outer;
	int bottom_range = rc.snap_edge_range_outer;
	int left_range = rc.snap_edge_range_outer;
	int right_range = rc.snap_edge_range_outer;
	if (wlr_output_layout_adjacent_output(seat->server->output_layout, WLR_DIRECTION_UP,
			output->wlr_output, cursor_x, cursor_y)) {
		top_range = rc.snap_edge_range_inner;
	}
	if (wlr_output_layout_adjacent_output(seat->server->output_layout, WLR_DIRECTION_DOWN,
			output->wlr_output, cursor_x, cursor_y)) {
		bottom_range = rc.snap_edge_range_inner;
	}
	if (wlr_output_layout_adjacent_output(seat->server->output_layout, WLR_DIRECTION_LEFT,
			output->wlr_output, cursor_x, cursor_y)) {
		left_range = rc.snap_edge_range_inner;
	}
	if (wlr_output_layout_adjacent_output(seat->server->output_layout, WLR_DIRECTION_RIGHT,
			output->wlr_output, cursor_x, cursor_y)) {
		right_range = rc.snap_edge_range_inner;
	}

	/* Translate into output local coordinates */
	wlr_output_layout_output_coords(seat->server->output_layout,
		output->wlr_output, &cursor_x, &cursor_y);

	struct wlr_box *area = &output->usable_area;

	int top = cursor_y - area->y;
	int bottom = area->y + area->height - cursor_y;
	int left = cursor_x - area->x;
	int right = area->x + area->width - cursor_x;

	if (top < top_range) {
		*edge1 = LAB_EDGE_TOP;
	} else if (bottom < bottom_range) {
		*edge1 = LAB_EDGE_BOTTOM;
	} else if (left < left_range) {
		*edge1 = LAB_EDGE_LEFT;
	} else if (right < right_range) {
		*edge1 = LAB_EDGE_RIGHT;
	} else {
		return false;
	}

	if (*edge1 == LAB_EDGE_TOP || *edge1 == LAB_EDGE_BOTTOM) {
		if (left < rc.snap_edge_corner_range) {
			*edge2 = LAB_EDGE_LEFT;
		} else if (right < rc.snap_edge_corner_range) {
			*edge2 = LAB_EDGE_RIGHT;
		}
	} else if (*edge1  == LAB_EDGE_LEFT || *edge1 == LAB_EDGE_RIGHT) {
		if (top < rc.snap_edge_corner_range) {
			*edge2 = LAB_EDGE_TOP;
		} else if (bottom < rc.snap_edge_corner_range) {
			*edge2 = LAB_EDGE_BOTTOM;
		}
	}

	return true;
}

/* Returns true if view was snapped to any edge */
static bool
snap_to_edge(struct view *view)
{
	struct output *output;
	enum lab_edge edge1, edge2;
	if (!edge_from_cursor(&view->server->seat, &output, &edge1, &edge2)) {
		return false;
	}
	enum lab_edge edge = edge1 | edge2;

	view_set_output(view, output);
	/*
	 * Don't store natural geometry here (it was
	 * stored already in interactive_begin())
	 */
	if (edge == LAB_EDGE_TOP && rc.snap_top_maximize) {
		/* <topMaximize> */
		view_maximize(view, VIEW_AXIS_BOTH,
			/*store_natural_geometry*/ false);
	} else {
		view_snap_to_edge(view, edge, /*across_outputs*/ false,
			/*combine*/ false, /*store_natural_geometry*/ false);
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

	overlay_finish(&view->server->seat);

	resize_indicator_hide(view);

	view->server->grabbed_view = NULL;

	/* Restore keyboard/pointer focus */
	seat_focus_override_end(&view->server->seat);
}
