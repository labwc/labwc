// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <limits.h>
#include "common/border.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "resistance.h"
#include "view.h"

static void
is_within_resistance_range(struct border view, struct border target,
		struct border other, struct border *flags, int strength)
{
	if (view.left >= other.left) {
		const int lo = other.left - abs(strength);
		const int hi = other.left - MIN(strength, 0);
		flags->left = target.left >= lo && target.left < hi;
	}

	if (!flags->left && view.right <= other.right) {
		const int lo = other.right + MIN(strength, 0);
		const int hi = other.right + abs(strength);
		flags->right = target.right > lo && target.right <= hi;
	}

	if (view.top >= other.top) {
		const int lo = other.top - abs(strength);
		const int hi = other.top - MIN(strength, 0);
		flags->top = target.top >= lo && target.top < hi;
	}

	if (!flags->top && view.bottom <= other.bottom) {
		const int lo = other.bottom + MIN(strength, 0);
		const int hi = other.bottom + abs(strength);
		flags->bottom = target.bottom > lo && target.bottom <= hi;
	}
}

static void
build_view_edges(struct view *view, struct wlr_box new_geom,
		struct border *view_edges, struct border *target_edges, bool move)
{
	struct border border = ssd_get_margin(view->ssd);

	/* Use the effective height to properly snap shaded views */
	int eff_height = view_effective_height(view, /* use_pending */ false);

	view_edges->left = view->current.x - border.left + (move ? 1 : 0);
	view_edges->top = view->current.y - border.top + (move ? 1 : 0);
	view_edges->right = view->current.x + view->current.width + border.right;
	view_edges->bottom = view->current.y + eff_height + border.bottom;

	target_edges->left = new_geom.x - border.left;
	target_edges->top = new_geom.y - border.top;
	target_edges->right = new_geom.x + new_geom.width + border.right;
	target_edges->bottom = new_geom.y + new_geom.height + border.bottom;
}

static void
update_nearest_edge(struct border view_edges, struct border target_edges,
		struct border region_edges, int strength,
		struct border *next_edges)
{
	struct border flags = { 0 };
	is_within_resistance_range(view_edges,
		target_edges, region_edges, &flags, strength);

	if (flags.left == 1) {
		next_edges->left = MAX(region_edges.left, next_edges->left);
	} else if (flags.right == 1) {
		next_edges->right = MIN(region_edges.right, next_edges->right);
	}

	if (flags.top == 1) {
		next_edges->top = MAX(region_edges.top, next_edges->top);
	} else if (flags.bottom == 1) {
		next_edges->bottom = MIN(region_edges.bottom, next_edges->bottom);
	}
}

static void
find_neighbor_edges(struct view *view, struct wlr_box new_geom,
		struct border *next_edges, bool move)
{
	if (rc.window_edge_strength == 0) {
		return;
	}

	struct border view_edges = { 0 };
	struct border target_edges = { 0 };

	build_view_edges(view, new_geom, &view_edges, &target_edges, move);

	struct view *v;
	for_each_view(v, &view->server->views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v == view || !output_is_usable(v->output)) {
			continue;
		}

		struct border border = ssd_get_margin(v->ssd);

		/*
		 * The significance of window edges here is inverted with
		 * respect to the usual orientation, because the edges of the
		 * view v of interest are those that would be encountered by a
		 * change in geometry in view along the named edge of view.
		 * Hence, when moving or resizing view *left*, it is the
		 * *right* edge of v that would be encountered, and vice versa;
		 * when moving or resizing view *down* ("bottom"), it is the
		 * *top* edge of v that would be encountered, and vice versa.
		 */
		struct border win_edges = {
			.top = v->current.y + v->current.height + border.bottom,
			.right = v->current.x - border.left,
			.bottom = v->current.y - border.top,
			.left = v->current.x + v->current.width + border.right,
		};

		update_nearest_edge(view_edges, target_edges,
			win_edges, rc.window_edge_strength, next_edges);
	}
}

static void
find_screen_edges(struct view *view, struct wlr_box new_geom,
		struct border *next_edges, bool move)
{
	if (rc.screen_edge_strength == 0) {
		return;
	}

	struct border view_edges = { 0 };
	struct border target_edges = { 0 };

	build_view_edges(view, new_geom, &view_edges, &target_edges, move);

	struct output *output;
	wl_list_for_each(output, &view->server->outputs, link) {
		if (!output_is_usable(output)) {
			continue;
		}

		struct wlr_box mgeom =
			output_usable_area_in_layout_coords(output);

		struct wlr_box ol;
		if (!wlr_box_intersection(&ol, &view->current, &mgeom) &&
				!wlr_box_intersection(&ol, &new_geom, &mgeom)) {
			continue;
		}

		struct border screen_edges = {
			.top = mgeom.y,
			.right = mgeom.x + mgeom.width,
			.bottom = mgeom.y + mgeom.height,
			.left = mgeom.x,
		};

		update_nearest_edge(view_edges, target_edges,
			screen_edges, rc.screen_edge_strength, next_edges);
	}
}

void
resistance_move_apply(struct view *view, double *x, double *y)
{
	assert(view);

	struct border border = ssd_get_margin(view->ssd);

	struct border next_edges = {
		.top = INT_MIN,
		.right = INT_MAX,
		.bottom = INT_MAX,
		.left = INT_MIN,
	};

	struct wlr_box new_geom = {
		.x = *x,
		.y = *y,
		.width = view->current.width,
		.height = view->current.height,
	};

	find_screen_edges(view, new_geom, &next_edges, /* move */ true);
	find_neighbor_edges(view, new_geom, &next_edges, /* move */ true);

	if (next_edges.left > INT_MIN) {
		*x = next_edges.left + border.left;
	} else if (next_edges.right < INT_MAX) {
		*x = next_edges.right - view->current.width - border.right;
	}

	if (next_edges.top > INT_MIN) {
		*y = next_edges.top + border.top;
	} else if (next_edges.bottom < INT_MAX) {
		*y = next_edges.bottom - border.bottom
			- view_effective_height(view, /* use_pending */ false);
	}
}

void
resistance_resize_apply(struct view *view, struct wlr_box *new_geom)
{
	assert(view);
	assert(!view->shaded);

	struct border border = ssd_get_margin(view->ssd);

	struct border next_edges = {
		.top = INT_MIN,
		.right = INT_MAX,
		.bottom = INT_MAX,
		.left = INT_MIN,
	};

	find_screen_edges(view, *new_geom, &next_edges, /* move */ false);
	find_neighbor_edges(view, *new_geom, &next_edges, /* move */ false);

	if (view->server->resize_edges & WLR_EDGE_LEFT) {
		if (next_edges.left > INT_MIN) {
			new_geom->x = next_edges.left + border.left;
			new_geom->width = view->current.width
				+ view->current.x - new_geom->x;
		}
	} else if (view->server->resize_edges & WLR_EDGE_RIGHT) {
		if (next_edges.right < INT_MAX) {
			new_geom->width = next_edges.right
				- view->current.x - border.right;
		}
	}

	if (view->server->resize_edges & WLR_EDGE_TOP) {
		if (next_edges.top > INT_MIN) {
			new_geom->y = next_edges.top + border.top;
			new_geom->height = view->current.height
				+ view->current.y - new_geom->y;
		}
	} else if (view->server->resize_edges & WLR_EDGE_BOTTOM) {
		if (next_edges.bottom < INT_MAX) {
			new_geom->height = next_edges.bottom
				- view->current.y - border.bottom;
		}
	}
}
