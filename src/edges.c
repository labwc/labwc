// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <limits.h>
#include <pixman.h>
#include <wlr/util/edges.h>
#include <wlr/util/box.h>
#include "common/border.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "edges.h"
#include "labwc.h"
#include "view.h"
#include "node.h"

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

static inline struct edge
build_edge(struct border region, enum wlr_edges direction, int pad)
{
	struct edge edge = { 0 };

	switch (direction) {
	case WLR_EDGE_LEFT:
		edge.offset = clipped_sub(region.left, pad);
		edge.min = region.top;
		edge.max = region.bottom;
		break;
	case WLR_EDGE_RIGHT:
		edge.offset = clipped_add(region.right, pad);
		edge.min = region.top;
		edge.max = region.bottom;
		break;
	case WLR_EDGE_TOP:
		edge.offset = clipped_sub(region.top, pad);
		edge.min = region.left;
		edge.max = region.right;
		break;
	case WLR_EDGE_BOTTOM:
		edge.offset = clipped_add(region.bottom, pad);
		edge.min = region.left;
		edge.max = region.right;
		break;
	case WLR_EDGE_NONE:
		/* Should never be reached */
		wlr_log(WLR_ERROR, "invalid direction");
		abort();
	}

	return edge;
}

static inline bool
is_lesser(enum wlr_edges direction)
{
	return direction == WLR_EDGE_LEFT || direction == WLR_EDGE_TOP;
}

static inline struct edge
build_visible_edge(struct border region, enum wlr_edges direction,
		int pad, uint32_t edges_visible)
{
	struct edge edge = build_edge(region, direction, pad);

	if (!(edges_visible & direction)) {
		edge.offset = is_lesser(direction) ? INT_MIN : INT_MAX;
	}

	return edge;
}

static void
validate_single_region_edge(int *valid_edge,
		struct border view, struct border target,
		struct border region, edge_validator_t validator,
		enum wlr_edges direction, uint32_t edges_visible)
{
	/*
	 * When a view snaps to another while moving to its target, it can do
	 * so in two ways: a view edge can snap to an "opposing" edge of the
	 * region (left <-> right, top <-> bottom) or to an "aligned" edge
	 * (left <-> left, right <-> right, top <-> top, bottom <-> bottom).
	 *
	 * When a view hits the opposing edge of a region, it should be
	 * separated by any configured gap and will resist *entry* into the
	 * region; when a view hits the aligned edge, it should not be
	 * separated by a gap and will resist *departure* from the region. The
	 * view and its target already include necessary padding to reflect the
	 * gap. The region does not. To make sure the "aligned" edges are
	 * properly aligned with respect to the configured gap, add padding to
	 * the region borders for aligned edges only.
	 */

	enum wlr_edges opposing = WLR_EDGE_NONE;

	switch (direction) {
	case WLR_EDGE_TOP:
		opposing = WLR_EDGE_BOTTOM;
		break;
	case WLR_EDGE_BOTTOM:
		opposing = WLR_EDGE_TOP;
		break;
	case WLR_EDGE_LEFT:
		opposing = WLR_EDGE_RIGHT;
		break;
	case WLR_EDGE_RIGHT:
		opposing = WLR_EDGE_LEFT;
		break;
	case WLR_EDGE_NONE:
		/* Should never be reached */
		assert(false);
		return;
	}

	validator(valid_edge,
		build_edge(view, direction, 0),
		build_edge(target, direction, 0),
		build_visible_edge(region, opposing, 0, edges_visible),
		build_visible_edge(region, direction, rc.gap, edges_visible),
		is_lesser(direction));
}

static void
validate_edges(struct border *valid_edges,
		struct border view, struct border target,
		struct border region, uint32_t edges_visible,
		edge_validator_t validator)
{
	/* Check for edges encountered during movement of left edge */
	validate_single_region_edge(&valid_edges->left,
		view, target, region, validator, WLR_EDGE_LEFT, edges_visible);

	/* Check for edges encountered during movement of right edge */
	validate_single_region_edge(&valid_edges->right,
		view, target, region, validator, WLR_EDGE_RIGHT, edges_visible);

	/* Check for edges encountered during movement of top edge */
	validate_single_region_edge(&valid_edges->top,
		view, target, region, validator, WLR_EDGE_TOP, edges_visible);

	/* Check for edges encountered during movement of bottom edge */
	validate_single_region_edge(&valid_edges->bottom,
		view, target, region, validator, WLR_EDGE_BOTTOM, edges_visible);
}

static void
validate_single_output_edge(int *valid_edge,
		struct border view, struct border target,
		struct border region, edge_validator_t validator,
		enum wlr_edges direction)
{
	static struct border unbounded = {
		.top = INT_MIN,
		.right = INT_MAX,
		.bottom = INT_MAX,
		.left = INT_MIN,
	};

	validator(valid_edge,
		build_edge(view, direction, 0),
		build_edge(target, direction, 0),
		build_edge(region, direction, 0),
		build_edge(unbounded, direction, 0), is_lesser(direction));
}

static void
validate_output_edges(struct border *valid_edges,
		struct border view, struct border target,
		struct wlr_box usable, edge_validator_t validator)
{
	/*
	 * When a view snaps to an output that contains it, it can be
	 * transformed into either of two equivalent problems:
	 *
	 * 1. The output region can be treated as if it were bounded by four
	 * half-planes, one sharing each edge of the view and extending
	 * infinitely *away* from the output. The moving view should then be
	 * tested as it encounters the "opposing" edge of each external region.
	 *
	 * 2. The output region can be treated as if it were composed of four
	 * half-planes, one sharing each edge of the view and extending
	 * infinitely to *overlap* the output. The moving view should then be
	 * tested as it encounters the "aligned" edge of each overlapping
	 * region.
	 *
	 * Either one of these problems can be realized by four calls to
	 * validate_edges with suitably defined half-plane regions, but most of
	 * the work in those validations will just be comparing invalid
	 * infinite edges.
	 *
	 * To save a bit of effort, just choose Problem 1 and directly validate
	 * only the non-infinite edges.
	 */

	struct border output = {
		.top = usable.y,
		.right = usable.x + usable.width,
		.bottom = usable.y + usable.height,
		.left = usable.x,
	};

	/* Left edge encounters a half-infinite region to the left of the output */

	validate_single_output_edge(&valid_edges->left,
			view, target, output, validator, WLR_EDGE_LEFT);

	/* Right edge encounters a half-infinite region to the right of the output */

	validate_single_output_edge(&valid_edges->right,
			view, target, output, validator, WLR_EDGE_RIGHT);

	/* Top edge encounters a half-infinite region above the output */

	validate_single_output_edge(&valid_edges->top,
			view, target, output, validator, WLR_EDGE_TOP);

	/* Bottom edge encounters a half-infinite region below the output */
	validate_single_output_edge(&valid_edges->bottom,
			view, target, output, validator, WLR_EDGE_BOTTOM);
}

/* Test if parts of the current view is covered by the remaining space in the region */
static void
subtract_view_from_space(struct view *view, pixman_region32_t *available)
{
	struct wlr_box view_size = ssd_max_extents(view);
	pixman_box32_t view_rect = {
		.x1 = view_size.x,
		.x2 = view_size.x + view_size.width,
		.y1 = view_size.y,
		.y2 = view_size.y + view_size.height
	};

	pixman_region_overlap_t overlap =
		pixman_region32_contains_rectangle(available, &view_rect);

	switch (overlap) {
	case PIXMAN_REGION_IN:
		view->edges_visible = WLR_EDGE_TOP | WLR_EDGE_RIGHT
			| WLR_EDGE_BOTTOM | WLR_EDGE_LEFT;
		break;
	case PIXMAN_REGION_OUT:
		view->edges_visible = 0;
		return;
	case PIXMAN_REGION_PART:
		; /* works around "a label can only be part of a statement" */
		pixman_region32_t intersection;
		pixman_region32_init(&intersection);
		pixman_region32_intersect_rect(&intersection, available,
			view_size.x, view_size.y,
			view_size.width, view_size.height);

		int nrects;
		const pixman_box32_t *rects =
			pixman_region32_rectangles(&intersection, &nrects);

		view->edges_visible = 0;
		for (int i = 0; i < nrects; i++) {
			if (rects[i].x1 == view_rect.x1) {
				view->edges_visible |= WLR_EDGE_LEFT;
			}
			if (rects[i].y1 == view_rect.y1) {
				view->edges_visible |= WLR_EDGE_TOP;
			}
			if (rects[i].x2 == view_rect.x2) {
				view->edges_visible |= WLR_EDGE_RIGHT;
			}
			if (rects[i].y2 == view_rect.y2) {
				view->edges_visible |= WLR_EDGE_BOTTOM;
			}
		}
		pixman_region32_fini(&intersection);
		break;
	}

	/* Subtract the view geometry from the available region for the next check */
	pixman_region32_t view_region;
	pixman_region32_init_rects(&view_region, &view_rect, 1);
	pixman_region32_subtract(available, available, &view_region);
	pixman_region32_fini(&view_region);
}

static void
subtract_node_tree(struct wlr_scene_tree *tree, pixman_region32_t *available,
		struct view *ignored_view)
{
	struct view *view;
	struct wlr_scene_node *node;
	struct node_descriptor *node_desc;
	wl_list_for_each_reverse(node, &tree->children, link) {
		if (!node->enabled) {
			/*
			 * This skips everything that is not being
			 * rendered, including minimized / unmapped
			 * windows and workspaces other than the
			 * current one.
			 */
			continue;
		}

		node_desc = node->data;
		if (node_desc && node_desc->type == LAB_NODE_DESC_VIEW) {
			view = node_view_from_node(node);
			if (view != ignored_view) {
				subtract_view_from_space(view, available);
			}
		} else if (node->type == WLR_SCENE_NODE_TREE) {
			subtract_node_tree(wlr_scene_tree_from_node(node),
				available, ignored_view);
		}
	}
}

void
edges_calculate_visibility(struct server *server, struct view *ignored_view)
{
	/*
	 * The region stores the available output layout space
	 * and subtracts the window geometries in reverse rendering
	 * order, e.g. a window rendered on top is subtracted first.
	 *
	 * This allows to detect if a window is actually visible.
	 * If there is no overlap of its geometry and the remaining
	 * region it must be completely covered by other windows.
	 *
	 */
	pixman_region32_t region;
	pixman_region32_init(&region);

	/*
	 * Initialize the region with each individual output.
	 *
	 * If we were to use NULL for the reference output we
	 * would get a single combined wlr_box of the whole
	 * layout which could cover actual invisible areas
	 * in case the output resolutions differ.
	 */
	struct output *output;
	struct wlr_box layout_box;
	wl_list_for_each(output, &server->outputs, link) {
		if (!output_is_usable(output)) {
			continue;
		}
		wlr_output_layout_get_box(server->output_layout,
			output->wlr_output, &layout_box);
		pixman_region32_union_rect(&region, &region,
			layout_box.x, layout_box.y, layout_box.width, layout_box.height);
	}

	subtract_node_tree(&server->scene->tree, &region, ignored_view);

	pixman_region32_fini(&region);
}

void
edges_find_neighbors(struct border *nearest_edges, struct view *view,
		struct wlr_box origin, struct wlr_box target,
		struct output *output, edge_validator_t validator, bool ignore_hidden)
{
	assert(view);
	assert(validator);
	assert(nearest_edges);

	if (!output_is_usable(view->output)) {
		wlr_log(WLR_DEBUG, "ignoring edge search for view on unusable output");
		return;
	}

	struct border view_edges = { 0 };
	struct border target_edges = { 0 };

	edges_for_target_geometry(&view_edges, view, origin);
	edges_for_target_geometry(&target_edges, view, target);

	struct view *v;
	for_each_view(v, &view->server->views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v == view || v->minimized || !output_is_usable(v->output)) {
			continue;
		}

		uint32_t edges_visible = ignore_hidden ? v->edges_visible :
			WLR_EDGE_TOP | WLR_EDGE_LEFT
				| WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;

		if (edges_visible == 0) {
			continue;
		}

		if (output && output != v->output && !view_on_output(v, output)) {
			continue;
		}

		/* Both view and v must share a common output */
		if (view->output != v->output && !(view->outputs & v->outputs)) {
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
			target_edges, win_edges, edges_visible, validator);
	}
}

void
edges_find_outputs(struct border *nearest_edges, struct view *view,
		struct wlr_box origin, struct wlr_box target,
		struct output *output, edge_validator_t validator)
{
	assert(view);
	assert(validator);
	assert(nearest_edges);

	if (!output_is_usable(view->output)) {
		wlr_log(WLR_DEBUG,
			"ignoring edge search for view on unusable output");
		return;
	}

	struct border view_edges = { 0 };
	struct border target_edges = { 0 };

	edges_for_target_geometry(&view_edges, view, origin);
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
		if (!wlr_box_intersection(&ol, &origin, &usable) &&
				!wlr_box_intersection(&ol, &target, &usable)) {
			continue;
		}

		validate_output_edges(nearest_edges,
			view_edges, target_edges, usable, validator);
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

static double
linear_interp(int x, int x1, int y1, int x2, int y2)
{
	/*
	 * For a line y = mx + b that passes through both (x1, y1) and
	 * (x2, y2), find and return the value y for a given point x.
	 *
	 * The point x does not need to fall in the range [x1, x2].
	 */

	/* No need to interpolate if line is horizontal */
	int rise = y2 - y1;
	if (rise == 0) {
		return y2;
	}

	/* For degenerate line, just pick a midpoint */
	int run = x2 - x1;
	if (run == 0) {
		return 0.5 * (y1 + y2);
	}

	/* Otherwise, linearly interpolate */
	int dx = x - x1;
	return y1 + dx * (rise / (double)run);
}

bool
edges_traverse_edge(struct edge current, struct edge target, struct edge obstacle)
{
	/*
	 * Each edge structure defines a line segment that can be represented
	 * in a local coordinate system as starting at (offset, min) and
	 * finishing at (offset, max).
	 *
	 * The starting and ending points of the "current" edge trace
	 * respective lines
	 *
	 *   1. (current.offset, current.min) -> (target.offset, target.min)
	 *   2. (current.offset, current.max) -> (target.offset, target.max)
	 *
	 * as the segment transits from its current position to its target.
	 * Hence, motion of the entire edge from current to target will sweep a
	 * quadrilateral bounded by (locally) vertical lines at current.offset
	 * and target.offset as well as the segments (1) and (2) above.
	 *
	 * To test if the motion will encounter the obstacle edge, we need to
	 * test if any of the obstacle edge falls within this quadrilateral.
	 * Thus, we need to find the extent of the quadrilateral at the same
	 * offset as the obstacle: a segment with starting point
	 * (obstacle.offset, lo) and ending point (obstacle.offset, hi).
	 */

	double lo =
		linear_interp(obstacle.offset,
			current.offset, current.min, target.offset, target.min);

	/* Motion misses when obstacle ends above start of quad segment */
	if (obstacle.max <= lo) {
		return false;
	}

	double hi =
		linear_interp(obstacle.offset,
			current.offset, current.max, target.offset, target.max);

	/* Motion hits when obstacle starts above the end of quad segment */
	return obstacle.min < hi;
}
