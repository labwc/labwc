// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <limits.h>
#include <wlr/util/box.h>
#include "common/border.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "edges.h"
#include "labwc.h"
#include "view.h"

static void
edges_for_target_geometry(struct border *edges, struct view *view,
		struct wlr_box target)
{
	struct border border = ssd_get_margin(view->ssd);

	/* Use the effective height to properly handle shaded views */
	int eff_height = view->shaded ? 0 : target.height;

	edges->left = target.x - border.left - rc.gap;
	edges->top = target.y - border.top - rc.gap;
	edges->right = target.x + target.width + border.right + rc.gap;
	edges->bottom = target.y + eff_height + border.bottom + rc.gap;
}

void
edges_initialize(struct border *edges)
{
	assert(edges);
	edges->top = INT_MIN;
	edges->right = INT_MAX;
	edges->bottom = INT_MAX;
	edges->left = INT_MIN;
}

static void
validate_edges(struct border *valid_edges,
		struct border view, struct border target,
		struct border region, edge_validator_t validator)
{
	/* When a view snaps to a region while moving to its target, it can do
	 * so in two ways: a view edge can snap to an "opposing" edge of the
	 * region (left <-> right, top <-> bottom) or to an "aligned" edge
	 * (left <-> left, right <-> right, top <-> top, bottom <-> bottom).
	 *
	 * When a view hits the opposing edge of a region, it should be
	 * separated by a gap; when a view hits the aligned edge, it should not
	 * be separated. The view and its target already include necessary
	 * padding to reflect the gap. The region does not. To make sure the
	 * "aligned" edges are properly aligned, add padding to the region
	 * borders for aligned edges only.
	 */

	struct border region_pad = {
		.top = clipped_sub(region.top, rc.gap),
		.right = clipped_add(region.right, rc.gap),
		.bottom = clipped_add(region.bottom, rc.gap),
		.left = clipped_sub(region.left, rc.gap),
	};

	/* Check for edges encountered during movement of left edge */
	validator(&valid_edges->left, view.left, target.left,
		region.right, region_pad.left, /* lesser */ true);

	/* Check for edges encountered during movement of right edge */
	validator(&valid_edges->right, view.right, target.right,
		region.left, region_pad.right, /* lesser */ false);

	/* Check for edges encountered during movement of top edge */
	validator(&valid_edges->top, view.top, target.top,
		region.bottom, region_pad.top, /* lesser */ true);

	/* Check for edges encountered during movement of bottom edge */
	validator(&valid_edges->bottom, view.bottom, target.bottom,
		region.top, region_pad.bottom, /* lesser */ false);
}

void
edges_find_neighbors(struct border *nearest_edges, struct view *view,
		struct wlr_box target, struct output *output,
		edge_validator_t validator, bool use_pending)
{
	assert(view);
	assert(validator);
	assert(nearest_edges);

	struct border view_edges = { 0 };
	struct border target_edges = { 0 };

	edges_for_target_geometry(&view_edges, view,
		use_pending ? view->pending : view->current);
	edges_for_target_geometry(&target_edges, view, target);

	struct view *v;
	for_each_view(v, &view->server->views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v == view || !output_is_usable(v->output)) {
			continue;
		}

		if (output && v->output != output) {
			continue;
		}

		struct border border = ssd_get_margin(v->ssd);

		struct border win_edges = {
			.top = v->current.y - border.top,
			.left = v->current.x - border.left,
			.bottom = v->current.y + border.bottom
				+ view_effective_height(v, /* use_pending */ false),
			.right = v->current.x + v->current.width + border.right,
		};

		validate_edges(nearest_edges, view_edges,
			target_edges, win_edges, validator);
	}
}

void
edges_find_outputs(struct border *nearest_edges, struct view *view,
		struct wlr_box target, struct output *output,
		edge_validator_t validator, bool use_pending)
{
	assert(view);
	assert(validator);
	assert(nearest_edges);

	struct border view_edges = { 0 };
	struct border target_edges = { 0 };

	struct wlr_box *view_geom =
		use_pending ? &view->pending : &view->current;

	edges_for_target_geometry(&view_edges, view, *view_geom);
	edges_for_target_geometry(&target_edges, view, target);

	struct output *o;
	wl_list_for_each(o, &view->server->outputs, link) {
		if (!output_is_usable(o)) {
			continue;
		}

		if (output && o != output) {
			continue;
		}

		struct wlr_box usable =
			output_usable_area_in_layout_coords(o);

		struct wlr_box ol;
		if (!wlr_box_intersection(&ol, view_geom, &usable) &&
				!wlr_box_intersection(&ol, &target, &usable)) {
			continue;
		}

		/*
		 * Split a single "leaving output" problem into four "entering
		 * complementary region" problems, treating the view, its
		 * target and the screen boundaries as half planes. This
		 * prevents unexpected snapping behavior like the bottom of a
		 * window snapping above the top of an output, where it would
		 * become invisible.
		 */
		struct border screen;
		struct border view_eff;
		struct border target_eff;

		/* First problem: view toward upper half-plane */
		edges_initialize(&screen);
		edges_initialize(&view_eff);
		edges_initialize(&target_eff);

		screen.bottom = usable.y;
		view_eff.top = view_edges.top;
		target_eff.top = target_edges.top;
		validate_edges(nearest_edges, view_eff, target_eff, screen, validator);

		/* Second problem: view toward lower half-plane */
		edges_initialize(&screen);
		edges_initialize(&view_eff);
		edges_initialize(&target_eff);

		screen.top = usable.y + usable.height;
		view_eff.bottom = view_edges.bottom;
		target_eff.bottom = target_edges.bottom;
		validate_edges(nearest_edges, view_eff, target_eff, screen, validator);

		/* Third problem: view toward left half-plane */
		edges_initialize(&screen);
		edges_initialize(&view_eff);
		edges_initialize(&target_eff);

		screen.right = usable.x;
		view_eff.left = view_edges.left;
		target_eff.left = target_edges.left;
		validate_edges(nearest_edges, view_eff, target_eff, screen, validator);

		/* Fourth problem: view toward right half-plane */
		edges_initialize(&screen);
		edges_initialize(&view_eff);
		edges_initialize(&target_eff);

		screen.left = usable.x + usable.width;
		view_eff.right = view_edges.right;
		target_eff.right = target_edges.right;
		validate_edges(nearest_edges, view_eff, target_eff, screen, validator);
	}
}

static void
adjust_move_coords_1d(int *edge, int lesser, int lesser_offset,
		int greater, int greater_offset, bool decreasing)
{
	/* Default best candidate is not valid */
	int best = INT_MAX;

	if (BOUNDED_INT(lesser)) {
		/* A valid lesser edge is the always the first candidate */
		best = clipped_add(lesser, lesser_offset);
	}

	if (BOUNDED_INT(greater)) {
		/* Check if a valid greater edge is a better candidate */
		best = edge_get_best(best,
			clipped_sub(greater, greater_offset), decreasing);
	}

	if (BOUNDED_INT(best)) {
		/* Replace the edge if a valid candidate was found */
		*edge = best;
	}
}

void
edges_adjust_move_coords(struct view *view, struct border edges,
		int *x, int *y, bool use_pending)
{
	assert(view);

	struct border border = ssd_get_margin(view->ssd);
	struct wlr_box *view_geom =
		use_pending ? &view->pending : &view->current;

	/* When moving, limit motion to the best valid, intervening edge */

	if (view_geom->x != *x) {
		int lshift = border.left + rc.gap;
		int rshift = border.right + rc.gap + view->pending.width;

		adjust_move_coords_1d(x, edges.left, lshift,
			edges.right, rshift, *x < view_geom->x);
	}

	if (view_geom->y != *y) {
		int tshift = border.top + rc.gap;
		int bshift = border.bottom + rc.gap
			+ view_effective_height(view, /* use_pending */ true);

		adjust_move_coords_1d(y, edges.top, tshift,
			edges.bottom, bshift, *y < view_geom->y);
	}
}

void
edges_adjust_resize_geom(struct view *view, struct border edges,
		uint32_t resize_edges, struct wlr_box *geom, bool use_pending)
{
	assert(view);

	struct border border = ssd_get_margin(view->ssd);
	struct wlr_box *view_geom =
		use_pending ? &view->pending : &view->current;

	/*
	 * When resizing along a given edge, limit the motion of that edge to
	 * any valid nearest edge in the corresponding direction.
	 */

	if (resize_edges & WLR_EDGE_LEFT) {
		if (BOUNDED_INT(edges.left)) {
			geom->x = edges.left + border.left + rc.gap;
			geom->width = view_geom->width + view_geom->x - geom->x;
		}
	} else if (resize_edges & WLR_EDGE_RIGHT) {
		if (BOUNDED_INT(edges.right)) {
			geom->width = edges.right
				- view_geom->x - border.right - rc.gap;
		}
	}

	if (resize_edges & WLR_EDGE_TOP) {
		if (BOUNDED_INT(edges.top)) {
			geom->y = edges.top + border.top + rc.gap;
			geom->height = view_geom->height + view_geom->y - geom->y;
		}
	} else if (resize_edges & WLR_EDGE_BOTTOM) {
		if (BOUNDED_INT(edges.bottom)) {
			geom->height = edges.bottom
				- view_geom->y - border.bottom - rc.gap;
		}
	}
}
