// SPDX-License-Identifier: GPL-2.0-only
#include <strings.h>
#include <wlr/util/box.h>
#include "labwc.h"
#include "snap.h"
#include "view.h"
#include "workspaces.h"

/* We cannot use MIN/MAX macros, as they may call functions twice, and
 * can be overridden by previous #define.
 */
static inline int
min(int a, int b) {
	return a < b ? a : b;
}

static inline int
max(int a, int b) {
	return a > b ? a : b;
}

static inline int
min3(int a, int b, int c) {
	return min(min(a, b), c);
}

enum snap_mode {
	SNAP_MODE_MOVE = 0,
	SNAP_MODE_GROW,
	SNAP_MODE_SHRINK,
};

static struct border
snap_get_view_edge(struct view *view)
{
	struct border margin = ssd_get_margin(view->ssd);
	struct border edge = {
		.left   = view->pending.x - margin.left,
		.top    = view->pending.y - margin.top,
		.right  = view->pending.x + view->pending.width + margin.right,
		.bottom = view->pending.y
			+ view_effective_height(view, true) + margin.bottom
	};
	return edge;
}

struct border
snap_get_max_distance(struct view *view)
{
	struct output *output = view->output;
	struct border margin = ssd_get_margin(view->ssd);
	struct wlr_box usable = output_usable_area_scaled(output);
	struct border distance = {
		.left   = usable.x + margin.left + rc.gap - view->pending.x,
		.top    = usable.y + margin.top  + rc.gap - view->pending.y,
		.right  = usable.x + usable.width
			- view->pending.width
			- margin.right  - rc.gap - view->pending.x,
		.bottom = usable.y + usable.height
			- view_effective_height(view, true)
			- margin.bottom - rc.gap - view->pending.y
	};
	return distance;
}

struct snap_search {
	const int search_dir; /* -1: left/up, 1: right/down */

	const int add_view_x;
	const int add_view_y;
	const int add_view_width;
	const int add_view_height;

	const int add_margin_left;
	const int add_margin_top;
	const int add_margin_right;
	const int add_margin_bottom;
};

/* near/far is the left, right, top or bottom border of a window,
 * depending on the search direction:
 *  - near_right: search to the right, snap to left  (near) border of a window.
 *  - far_right:  search to the right, snap to right (far)  border of a window.
 *  - near_left:  search to the left,  snap to right (near) border of a window.
 *  - far_left:   search to the left,  snap to left  (far)  border of a window.
 *
 * structs below define what coordinates and margins to take into
 * account depending near/far, and direction.
 */
static const struct snap_search near_left  = { -1,   1, 0, 1, 0,    0,  0,  1,  0 };
static const struct snap_search near_up    = { -1,   0, 1, 0, 1,    0,  0,  0,  1 };
static const struct snap_search near_right = {  1,   1, 0, 0, 0,   -1,  0,  0,  0 };
static const struct snap_search near_down  = {  1,   0, 1, 0, 0,    0, -1,  0,  0 };
static const struct snap_search far_left   = { -1,   1, 0, 0, 0,   -1,  0,  0,  0 };
static const struct snap_search far_up     = { -1,   0, 1, 0, 0,    0, -1,  0,  0 };
static const struct snap_search far_right  = {  1,   1, 0, 1, 0,    0,  0,  1,  0 };
static const struct snap_search far_down   = {  1,   0, 1, 0, 1,    0,  0,  1,  0 };

static inline int
_snap_next_edge(struct view *view, int start_pos, const struct snap_search def, int max, int gap)
{
	struct output *output = view->output;
	struct server *server = output->server;
	struct view *v;
	int p = max;
	for_each_view(v, &server->views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v == view || v->output != output || v->minimized
				|| v->maximized == VIEW_AXIS_BOTH) {
			continue;
		}

		struct border margin = ssd_get_margin(v->ssd);
		int vp = -start_pos;
		vp += def.add_margin_left   * margin.left;
		vp += def.add_margin_top    * margin.top;
		vp += def.add_margin_right  * margin.right;
		vp += def.add_margin_bottom * margin.bottom;
		vp += def.add_view_x        * v->pending.x;
		vp += def.add_view_y        * v->pending.y;
		vp += def.add_view_width    * v->pending.width;
		vp += def.add_view_height   * view_effective_height(v, true);
		vp += gap;

		if (def.search_dir * vp > 0 && def.search_dir * (vp - p) < 0) {
			p = vp;
		}
	}
	return p;
}

static void
_snap_move_resize_to_edge(struct view *view, enum view_edge direction, enum snap_mode mode,
		struct wlr_box *delta)
{
	struct border edge = snap_get_view_edge(view);
	struct border dmax;

	if (mode == SNAP_MODE_SHRINK) {
		/* limit to half of current size */
		int eff_height = view_effective_height(view, true);
		int width_max_dx  = max(view->pending.width  - LAB_MIN_VIEW_WIDTH,  0);
		int height_max_dy = max(eff_height - LAB_MIN_VIEW_HEIGHT, 0);
		dmax.right  = min(width_max_dx,  view->pending.width  / 2);
		dmax.bottom = min(height_max_dy, eff_height / 2);
		dmax.left   = -dmax.right;
		dmax.top    = -dmax.bottom;
	} else {
		dmax = snap_get_max_distance(view);
	}

	switch (direction) {
	case VIEW_EDGE_LEFT:
		if (mode == SNAP_MODE_MOVE) {
			delta->x += max(
				/* left edge to left/right edges */
				_snap_next_edge(view, edge.left, near_left, dmax.left, rc.gap),
				_snap_next_edge(view, edge.left, far_left,  dmax.left, 0)
			);
		} else if (mode == SNAP_MODE_GROW) {
			int dx = max(
				/* left edge to left/right edges */
				_snap_next_edge(view, edge.left, near_left, dmax.left, rc.gap),
				_snap_next_edge(view, edge.left, far_left,  dmax.left, 0)
			);
			delta->x     +=  dx;
			delta->width += -dx;
		} else if (mode == SNAP_MODE_SHRINK) {
			delta->width += max(
				/* right edge to left/right edges */
				_snap_next_edge(view, edge.right, near_left, dmax.left, 0),
				_snap_next_edge(view, edge.right, far_left,  dmax.left, -rc.gap)
			);
		}
		break;
	case VIEW_EDGE_UP:
		if (mode == SNAP_MODE_MOVE) {
			delta->y += max(
				/* top edge to top/bottom edges */
				_snap_next_edge(view, edge.top, near_up, dmax.top, rc.gap),
				_snap_next_edge(view, edge.top, far_up,  dmax.top, 0)
			);
		} else if (mode == SNAP_MODE_GROW) {
			int dy = max(
				/* top edge to top/bottom edges */
				_snap_next_edge(view, edge.top, near_up, dmax.top, rc.gap),
				_snap_next_edge(view, edge.top, far_up,  dmax.top, 0)
			);
			delta->y      +=  dy;
			delta->height += -dy;
		} else if (mode == SNAP_MODE_SHRINK) {
			delta->height += max(
				/* bottom edge to top/bottom edges */
				_snap_next_edge(view, edge.bottom, near_up, dmax.top, 0),
				_snap_next_edge(view, edge.bottom, far_up,  dmax.top, -rc.gap)
			);
		}
		break;
	case VIEW_EDGE_RIGHT:
		if (mode == SNAP_MODE_MOVE) {
			delta->x += min3(
				/* left edge to left/right edges */
				_snap_next_edge(view, edge.left, near_right, dmax.right, 0),
				_snap_next_edge(view, edge.left, far_right,  dmax.right, rc.gap),
				/* right edge to left edge */
				_snap_next_edge(view, edge.right, near_right, dmax.right, -rc.gap)
			);
		} else if (mode == SNAP_MODE_GROW) {
			delta->width += min(
				/* right edge to left/right edges */
				_snap_next_edge(view, edge.right, near_right, dmax.right, -rc.gap),
				_snap_next_edge(view, edge.right, far_right,  dmax.right, 0)
			);
		} else if (mode == SNAP_MODE_SHRINK) {
			delta->x += min(
				/* left edge to left/right edges */
				_snap_next_edge(view, edge.left, near_right, dmax.right, 0),
				_snap_next_edge(view, edge.left, far_right,  dmax.right, rc.gap)
			);
			delta->width += -(delta->x);
		}
		break;
	case VIEW_EDGE_DOWN:
		if (mode == SNAP_MODE_MOVE) {
			delta->y += min3(
				/* top edge to top/bottom edges */
				_snap_next_edge(view, edge.top, near_down, dmax.bottom, 0),
				_snap_next_edge(view, edge.top, far_down,  dmax.bottom, rc.gap),
				/* bottom edge to top edge */
				_snap_next_edge(view, edge.bottom, near_down, dmax.bottom, -rc.gap)
			);
		} else if (mode == SNAP_MODE_GROW) {
			delta->height += min(
				/* bottom edge to top/bottom edges */
				_snap_next_edge(view, edge.bottom, near_down, dmax.bottom, -rc.gap),
				_snap_next_edge(view, edge.bottom, far_down,  dmax.bottom, 0)
			);
		} else if (mode == SNAP_MODE_SHRINK) {
			delta->y += min(
				/* top edge to top/bottom edges */
				_snap_next_edge(view, edge.top, near_down, dmax.bottom, 0),
				_snap_next_edge(view, edge.top, far_down,  dmax.bottom, rc.gap)
			);
			delta->height += -(delta->y);
		}
		break;
	default:
		return;
	}
}

void
snap_vector_to_next_edge(struct view *view, enum view_edge direction, int *dx, int *dy)
{
	struct wlr_box delta = {0};
	_snap_move_resize_to_edge(view, direction, SNAP_MODE_MOVE, &delta);
	*dx = delta.x;
	*dy = delta.y;
}

int
snap_distance_to_next_edge(struct view *view, enum view_edge direction)
{
	struct wlr_box delta = {0};
	_snap_move_resize_to_edge(view, direction, SNAP_MODE_MOVE, &delta);
	switch (direction) {
	case VIEW_EDGE_LEFT:  return -delta.x;
	case VIEW_EDGE_UP:    return -delta.y;
	case VIEW_EDGE_RIGHT: return  delta.x;
	case VIEW_EDGE_DOWN:  return  delta.y;
	default: return 0;
	}
}

void
snap_grow_to_next_edge(struct view *view, enum view_edge direction, struct wlr_box *geo)
{
	_snap_move_resize_to_edge(view, direction, SNAP_MODE_GROW, geo);
}

void
snap_shrink_to_next_edge(struct view *view, enum view_edge direction, struct wlr_box *geo)
{
	_snap_move_resize_to_edge(view, direction, SNAP_MODE_SHRINK, geo);
}
