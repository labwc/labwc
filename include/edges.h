/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_EDGES_H
#define LABWC_EDGES_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include "common/macros.h"

struct border;
struct output;
struct server;
struct view;
struct wlr_box;

static inline int
clipped_add(int a, int b)
{
	if (b > 0) {
		return a >= (INT_MAX - b) ? INT_MAX : (a + b);
	} else if (b < 0) {
		return a <= (INT_MIN - b) ? INT_MIN : (a + b);
	}

	return a;
}

static inline int
clipped_sub(int a, int b)
{
	if (b > 0) {
		return a <= (INT_MIN + b) ? INT_MIN : (a - b);
	} else if (b < 0) {
		return a >= (INT_MAX + b) ? INT_MAX : (a - b);
	}

	return a;
}

static inline int
edge_get_best(int next, int edge, bool decreasing)
{
	if (!BOUNDED_INT(next)) {
		/* Any bounded edge beats an unbounded next */
		return BOUNDED_INT(edge) ? edge : next;
	}

	/* No unbounded edge ever beats next */
	if (!BOUNDED_INT(edge)) {
		return next;
	}

	/* Max edge wins for decreasing moves, min edge for increasing */
	return decreasing ? MAX(next, edge) : MIN(next, edge);
}

struct edge {
	/* Position of an edge along the axis perpendicular to it */
	int offset;

	/* Limits of edge along axis parallel to it */
	int min;
	int max;
};

/*
 * edge_validator_t - edge validator signature
 * @best: pointer to the current "best" edge
 * @current: current position of a moving edge
 * @target: position to which the moving edge will be moved
 * @oppose: opposing edge of encountered region
 * @align: aligned edge of encountered region
 * @lesser: true if the moving edge is top or left, false otherwise
 *
 * This function will be used by edge_find_neighbors and edge_find_outputs to
 * validate and select the "best" output or neighbor edge against which a
 * moving edge should be snapped. The moving edge has current position
 * "current" and desired position "target". The validator should determine
 * whether motion of the crosses the given opposed and aligned edges of a trial
 * region and should be considered a snap point.
 *
 * Opposing edges are on the opposite side of the target region from the moving
 * edge (i.e., left <-> right, top <-> bottom). When the moving edge snaps to
 * an opposing edge, the view should maintain the configured gap. Aligned edges
 * are on the same side of the target region from the moving edge (i.e.,
 * left <-> left, right <-> right, top <-> top, bottom <-> bottom). When the
 * moving edge snaps to an aligned edge, the view should *not* include a gap.
 *
 * If window gaps are configured, all edges will be offset as appropriate to
 * reflect the desired padding. Thus, the validator should generally compare
 * the given current or target values directly to the opposing and aligned edge
 * without regard for rc.gap.
 *
 * Any edge may take the values INT_MIN or INT_MAX to indicate that the edge
 * should be effectively ignored. Should the validator decide that a given
 * region edge (oppose or align) should be a preferred snap point, it should
 * update the value of *best accordingly.
 */
typedef void (*edge_validator_t)(int *best, struct edge current,
	struct edge target, struct edge oppose, struct edge align, bool lesser);

void edges_initialize(struct border *edges);

void edges_adjust_geom(struct view *view, struct border edges,
	uint32_t resize_edges, struct wlr_box *geom);

void edges_find_neighbors(struct border *nearest_edges, struct view *view,
	struct wlr_box origin, struct wlr_box target,
	struct output *output, edge_validator_t validator, bool ignore_hidden);

void edges_find_outputs(struct border *nearest_edges, struct view *view,
	struct wlr_box origin, struct wlr_box target,
	struct output *output, edge_validator_t validator);

void edges_adjust_move_coords(struct view *view, struct border edges,
	int *x, int *y, bool use_pending);

void edges_adjust_resize_geom(struct view *view, struct border edges,
	uint32_t resize_edges, struct wlr_box *geom, bool use_pending);

bool edges_traverse_edge(struct edge current, struct edge target, struct edge edge);

void edges_calculate_visibility(struct server *server, struct view *ignored_view);
#endif /* LABWC_EDGES_H */
