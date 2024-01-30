// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <limits.h>
#include "common/border.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "edges.h"
#include "labwc.h"
#include "resistance.h"
#include "view.h"

static void
check_edge(int *next, int current, int target,
		int oppose, int align, bool lesser, int tolerance)
{
	/* Ignore non-moving edges */
	if (current == target) {
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
	const bool decreasing = target < current;

	/* Check the opposing edge */
	bool valid = false;
	if (decreasing) {
		const int lo = clipped_sub(oppose, abs(tolerance));
		const int hi = clipped_sub(oppose, MIN(tolerance, 0));
		valid = target >= lo && target < hi;
	} else {
		/* Check for increasing movement across opposing edge */
		const int lo = clipped_add(oppose, MIN(tolerance, 0));
		const int hi = clipped_add(oppose, abs(tolerance));
		valid = target > lo && target <= hi;
	}

	if (valid) {
		*next = edge_get_best(*next, oppose, decreasing);
	}

	/* Check the aligned edge */
	valid = false;
	if (decreasing) {
		const int lo = clipped_sub(align, abs(tolerance));
		const int hi = clipped_sub(align, MIN(tolerance, 0));
		valid = target >= lo && target < hi;
	} else {
		const int lo = clipped_add(align, MIN(tolerance, 0));
		const int hi = clipped_add(align, abs(tolerance));
		valid = target > lo && target <= hi;
	}

	if (valid) {
		*next = edge_get_best(*next, align, decreasing);
	}
}

static void
check_edge_output(int *next, int current, int target,
		int oppose, int align, bool lesser)
{
	check_edge(next, current, target,
		oppose, align, lesser, rc.screen_edge_strength);
}

static void
check_edge_window(int *next, int current, int target,
		int oppose, int align, bool lesser)
{
	check_edge(next, current, target,
		oppose, align, lesser, rc.window_edge_strength);
}

void
resistance_move_apply(struct view *view, double *x, double *y)
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
		edges_find_outputs(&next_edges, view, target, NULL,
			check_edge_output, /* use_pending */ false);
	}

	if (rc.window_edge_strength != 0) {
		/* Find any relevant window edges encountered by this move */
		edges_find_neighbors(&next_edges, view, target, NULL,
			check_edge_window, /* use_pending */ false);
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

	if (rc.screen_edge_strength != 0) {
		/* Find any relevant output edges encountered by this move */
		edges_find_outputs(&next_edges, view, *new_geom, NULL,
			check_edge_output, /* use_pending */ false);
	}

	if (rc.window_edge_strength != 0) {
		/* Find any relevant window edges encountered by this move */
		edges_find_neighbors(&next_edges, view, *new_geom, NULL,
			check_edge_window, /* use_pending */ false);
	}

	/* If any "best" edges were encountered during this move, snap motion */
	edges_adjust_resize_geom(view, next_edges,
		view->server->resize_edges, new_geom, /* use_pending */ false);
}
