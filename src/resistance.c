// SPDX-License-Identifier: GPL-2.0-only
#include "resistance.h"
#include <assert.h>
#include <limits.h>
#include "common/border.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "edges.h"
#include "labwc.h"
#include "snap-constraints.h"
#include "view.h"

static void
check_edge(int *next, struct edge current, struct edge target,
		struct edge oppose, struct edge align, int tolerance, bool lesser)
{
	int cur = current.offset;
	int tgt = target.offset;
	int opp = oppose.offset;

	/* Ignore non-moving edges */
	if (cur == tgt) {
		return;
	}

	/*
	 * The edge defined by current and moving to target may encounter two
	 * edges of another region: the opposing edge of the region is that in
	 * the opposite orientation of the moving edge (i.e., left <-> right or
	 * top <-> bottom); the aligned edge of the region is that in the same
	 * orientation as the moving edge (i.e., left <->left, top <-> top,
	 * right <-> right, bottom <-> bottom).
	 *
	 * Any opposing or aligned edge of a region is considered "valid" in
	 * this search if the resist/attract zone (defined by tolerance) of
	 * that edge contains the target position of the moving edge.
	 */

	/* Direction of motion for the edge */
	const bool decreasing = tgt < cur;

	/*
	 * Motion resists "entry" into the space of another window, but never
	 * resist leaving it. Without edge attraction, this only happens when
	 * the "leading" edge of a motion (top edge upward, bottom edge
	 * downward, left edge leftward, right edge rightward) encounters an
	 * opposing edge. If the motion is not of a leading edge, there is no
	 * need to check for any resistance.
	 *
	 * However, if there is attraction, a "trailing" edge of a motion (top
	 * edge downward, bottom edge upward, left edge rightward, right edge
	 * leftward) may be grabbed by the opposing edge of another window as
	 * it passes. Hence, trailing edges still need to be tested in
	 * attractive cases.
	 */
	if (tolerance >= 0 && lesser != decreasing) {
		return;
	}

	/* Check the opposing edge */
	bool valid = false;
	if (decreasing) {
		/* Check for decreasing movement across opposing edge */
		const int lo = clipped_sub(opp, abs(tolerance));
		const int hi = clipped_sub(opp, MIN(tolerance, 0));
		valid = tgt >= lo && tgt < hi && cur >= opp;
	} else {
		/* Check for increasing movement across opposing edge */
		const int lo = clipped_add(opp, MIN(tolerance, 0));
		const int hi = clipped_add(opp, abs(tolerance));
		valid = tgt > lo && tgt <= hi && cur <= opp;
	}

	if (valid && edges_traverse_edge(current, target, oppose)) {
		*next = edge_get_best(*next, opp, decreasing);
	}
}

static void
check_edge_output(int *next, struct edge current, struct edge target,
		struct edge oppose, struct edge align, bool lesser)
{
	check_edge(next, current, target,
		oppose, align, rc.screen_edge_strength, lesser);
}

static void
check_edge_window(int *next, struct edge current, struct edge target,
		struct edge oppose, struct edge align, bool lesser)
{
	check_edge(next, current, target,
		oppose, align, rc.window_edge_strength, lesser);
}

bool
resistance_unsnap_apply(struct view *view, int *x, int *y)
{
	if (view_is_floating(view)) {
		return false;
	}

	int dx = *x - view->current.x;
	int dy = *y - view->current.y;
	if (view->maximized == VIEW_AXIS_HORIZONTAL) {
		if (abs(dx) < rc.unmaximize_threshold) {
			*x = view->current.x;
			return false;
		}
	} else if (view->maximized == VIEW_AXIS_VERTICAL) {
		if (abs(dy) < rc.unmaximize_threshold) {
			*y = view->current.y;
			return false;
		}
	} else {
		if (dx * dx + dy * dy < rc.unsnap_threshold * rc.unsnap_threshold) {
			*x = view->current.x;
			*y = view->current.y;
			return false;
		}
	}

	return true;
}

void
resistance_move_apply(struct view *view, int *x, int *y)
{
	assert(view);

	struct border next_edges;
	edges_initialize(&next_edges);

	struct wlr_box target = {
		.x = *x,
		.y = *y,
		.width = view->current.width,
		.height = view->current.height,
	};

	if (rc.screen_edge_strength != 0) {
		/* Find any relevant output edges encountered by this move */
		edges_find_outputs(&next_edges, view,
			view->current, target, NULL, check_edge_output);
	}

	if (rc.window_edge_strength != 0) {
		/* Find any relevant window edges encountered by this move */
		edges_find_neighbors(&next_edges, view, view->current, target,
			NULL, check_edge_window, /* ignore_hidden */ true);
	}

	/* If any "best" edges were encountered during this move, snap motion */
	edges_adjust_move_coords(view, next_edges,
		&target.x, &target.y, /* use_pending */ false);

	*x = target.x;
	*y = target.y;
}

void
resistance_resize_apply(struct view *view, struct wlr_box *new_geom)
{
	assert(view);
	assert(!view->shaded);

	struct border next_edges;
	edges_initialize(&next_edges);

	/* Use a constrained, effective geometry for snapping if appropriate */
	enum wlr_edges resize_edges = view->server->resize_edges;
	struct wlr_box origin =
		snap_constraints_effective(view, resize_edges, /* use_pending */ false);

	if (rc.screen_edge_strength != 0) {
		/* Find any relevant output edges encountered by this move */
		edges_find_outputs(&next_edges, view,
			origin, *new_geom, NULL, check_edge_output);
	}

	if (rc.window_edge_strength != 0) {
		/* Find any relevant window edges encountered by this move */
		edges_find_neighbors(&next_edges, view, origin, *new_geom,
			NULL, check_edge_window, /* ignore_hidden */ true);
	}

	/* If any "best" edges were encountered during this move, snap motion */
	edges_adjust_resize_geom(view, next_edges,
		resize_edges, new_geom, /* use_pending */ false);

	/*
	 * Record effective geometry after snapping in case the client opts to
	 * ignore or modify the configured geometry
	 */
	snap_constraints_set(view, resize_edges, *new_geom);
}
