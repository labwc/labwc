// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <limits.h>
#include <wlr/util/box.h>
#include "common/border.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "edges.h"
#include "labwc.h"
#include "resistance.h"
#include "snap.h"
#include "view.h"

static void
check_edge(int *next, struct edge current, struct edge target,
		struct edge oppose, struct edge align, bool lesser)
{
	int cur = current.offset;
	int tgt = target.offset;
	int opp = oppose.offset;
	int aln = align.offset;

	if (cur == tgt) {
		return;
	}

	/*
	 * The edge defined by current and moving to target may encounter two
	 * edges of another region: the opposing edge of the region is that in
	 * the opposite orientation of the moving edge (i.e., left <-> right or
	 * top <-> bottom); the aligned edge of the region is that in the same
	 * orientation as the moving edge (i.e., left <-> left, top <-> top,
	 * right <-> right, bottom <-> bottom).
	 *
	 * Any opposing or aligned edge of a region is considered "valid" in
	 * this search if the edge sits between the current and target
	 * positions of the moving edge (including the target position itself).
	 */

	/* Direction of motion for the edge */
	const bool decreasing = tgt < cur;

	/* Check the opposing edge */
	if ((tgt <= opp && opp < cur) || (cur < opp && opp <= tgt)) {
		*next = edge_get_best(*next, opp, decreasing);
	}

	/* Check the aligned edge */
	if ((tgt <= aln && aln < cur) || (cur < aln && aln <= tgt)) {
		*next = edge_get_best(*next, aln, decreasing);
	}
}

void
snap_move_to_edge(struct view *view, enum view_edge direction,
		bool snap_to_windows, int *dx, int *dy)
{
	assert(view);

	*dx = 0;
	*dy = 0;

	struct output *output = view->output;
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "view has no output, not snapping to edge");
		return;
	}

	struct wlr_box target = view->pending;
	struct border ssd = ssd_thickness(view);
	struct wlr_box usable = output_usable_area_in_layout_coords(output);

	/*
	 * First try to move the view to the relevant edge of its output. If
	 * the view is off-screen, such a move might actually run contrary to
	 * the commanded direction (e.g., a view off the screen to the left,
	 * when moved to the left edge, will actually move rightward). This is
	 * counter-intuitive, so abandon any such movements.
	 *
	 * In addition, any view that is already at the desired screen edge
	 * needs no further consideration.
	 */
	switch (direction) {
	case VIEW_EDGE_LEFT:
		target.x = usable.x + ssd.left + rc.gap;
		if (target.x >= view->pending.x) {
			return;
		}
		break;
	case VIEW_EDGE_RIGHT:
		target.x = usable.x + usable.width
			- rc.gap - target.width - ssd.right;
		if (target.x <= view->pending.x) {
			return;
		}
		break;
	case VIEW_EDGE_UP:
		target.y = usable.y + ssd.top + rc.gap;
		if (target.y >= view->pending.y) {
			return;
		}
		break;
	case VIEW_EDGE_DOWN:
		target.y = usable.y + usable.height - rc.gap - ssd.bottom
			- view_effective_height(view, /* use_pending */ true);
		if (target.y <= view->pending.y) {
			return;
		}
		break;
	default:
		return;
	}

	/*
	 * Because the target has been updated to put the view at the edge of
	 * an output, there is no need to check snapping to output edges. If
	 * snapping to view is desired, check for snapping against any view on
	 * the same output.
	 */
	if (snap_to_windows) {
		struct border next_edges;
		edges_initialize(&next_edges);

		edges_find_neighbors(&next_edges,
			view, target, output, check_edge,
			/* use_pending */ true, /* ignore_hidden */ false);

		/* If any "best" edges were encountered, limit motion */
		edges_adjust_move_coords(view, next_edges,
			&target.x, &target.y, /* use_pending */ true);
	}

	*dx = target.x - view->pending.x;
	*dy = target.y - view->pending.y;
}

void
snap_grow_to_next_edge(struct view *view, enum view_edge direction,
		struct wlr_box *geo)
{
	assert(view);
	assert(!view->shaded);

	*geo = view->pending;

	struct output *output = view->output;
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "view has no output, not growing to edge");
		return;
	}

	struct border ssd = ssd_thickness(view);
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	uint32_t resize_edges;

	/* First try to grow the view to the relevant edge of its output. */
	switch (direction) {
	case VIEW_EDGE_LEFT:
		geo->x = usable.x + ssd.left + rc.gap;
		geo->width = view->pending.x + view->pending.width - geo->x;
		resize_edges = WLR_EDGE_LEFT;
		break;
	case VIEW_EDGE_RIGHT:
		geo->width = usable.x + usable.width
			- rc.gap - ssd.right - view->pending.x;
		resize_edges = WLR_EDGE_RIGHT;
		break;
	case VIEW_EDGE_UP:
		geo->y = usable.y + ssd.top + rc.gap;
		geo->height = view->pending.y + view->pending.height - geo->y;
		resize_edges = WLR_EDGE_TOP;
		break;
	case VIEW_EDGE_DOWN:
		geo->height = usable.y + usable.height
			- rc.gap - ssd.bottom - view->pending.y;
		resize_edges = WLR_EDGE_BOTTOM;
		break;
	default:
		return;
	}

	/* No grow operation should ever shrink the view */
	if (geo->width < view->pending.width ||
			geo->height < view->pending.height) {
		*geo = view->pending;
		return;
	}

	/* If the view doesn't change size, there is no need for snap checks */
	if (geo->width == view->pending.width &&
			geo->height == view->pending.height) {
		*geo = view->pending;
		return;
	}

	struct border next_edges;
	edges_initialize(&next_edges);

	/* Limit motion to any intervening edge of other views on this output */
	edges_find_neighbors(&next_edges,
		view, *geo, output, check_edge,
		/* use_pending */ true, /* ignore_hidden */ false);
	edges_adjust_resize_geom(view, next_edges,
		resize_edges, geo, /* use_pending */ true);
}

void
snap_shrink_to_next_edge(struct view *view, enum view_edge direction,
		struct wlr_box *geo)
{
	assert(view);
	assert(!view->shaded);

	*geo = view->pending;
	uint32_t resize_edges;

	/*
	 * First shrink the view along the relevant edge. The maximum shrink
	 * allowed is half the current size, but the window must also meet
	 * minimum size requirements.
	 */
	switch (direction) {
	case VIEW_EDGE_RIGHT:
		geo->width = MAX(geo->width / 2, LAB_MIN_VIEW_WIDTH);
		geo->x = view->pending.x + view->pending.width - geo->width;
		resize_edges = WLR_EDGE_LEFT;
		break;
	case VIEW_EDGE_LEFT:
		geo->width = MAX(geo->width / 2, LAB_MIN_VIEW_WIDTH);
		resize_edges = WLR_EDGE_RIGHT;
		break;
	case VIEW_EDGE_DOWN:
		geo->height = MAX(geo->height / 2, LAB_MIN_VIEW_HEIGHT);
		geo->y = view->pending.y + view->pending.height - geo->height;
		resize_edges = WLR_EDGE_TOP;
		break;
	case VIEW_EDGE_UP:
		geo->height = MAX(geo->height / 2, LAB_MIN_VIEW_HEIGHT);
		resize_edges = WLR_EDGE_BOTTOM;
		break;
	default:
		return;
	}

	/* If the view doesn't change size, abandon the shrink */
	if (geo->width == view->pending.width &&
			geo->height == view->pending.height) {
		*geo = view->pending;
		return;
	}

	struct border next_edges;
	edges_initialize(&next_edges);

	/* Snap to output edges if the moving edge started off-screen */
	edges_find_outputs(&next_edges, view, *geo,
		view->output, check_edge, /* use_pending */ true);

	/* Limit motion to any intervening edge of ther views on this output */
	edges_find_neighbors(&next_edges,
		view, *geo, view->output, check_edge,
		/* use_pending */ true, /* ignore_hidden */ false);

	edges_adjust_resize_geom(view, next_edges,
		resize_edges, geo, /* use_pending */ true);
}
