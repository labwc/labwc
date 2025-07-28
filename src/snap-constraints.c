// SPDX-License-Identifier: GPL-2.0-only
#include "snap-constraints.h"
#include <assert.h>
#include <wlr/util/box.h>
#include "common/macros.h"
#include "labwc.h"
#include "view.h"

/*
 * When snapping to edges during a resize action, XDG clients can override the
 * desired size to honor arbitrary sizing constraints (*e.g.*, honoring some
 * aspect ratio or ensuring that a terminal window consists of an integer
 * number of character cells). In that case, the final position of the view
 * edge may fall short of the edge to which it's snapped.
 *
 * This can prevent a subsequent resize action in the same direction from ever
 * crossing the "missed" edge, because the next action will keep trying to hit
 * the edge and the client will overriding the desired size. To compensate,
 * every snapped resize will update a "last snapped" cache, recording the
 * view that was resized, the position (and orientation) of the resized edge,
 * and the expected geometry resulting from the snapped resize. At first, the
 * expected geometry is the "pending" geometry that will be sent in a configure
 * request. However, if the client overrides this pending geometry with some
 * other, constrained value, the expectation should be updated (only once!) to
 * reflect the size that the client chooses to honor.
 *
 * In subsequent snapped resize actions, if:
 *
 * 1. The view is the same as in the last attempted snapped resize;
 * 2. The direction of resizing is the same as in the last attempt; and
 * 3. The geometry of the view matches that expected from the last attempt;
 *
 * then the view geometry will be modified to reflect the *original* intended
 * geometry from last snapped resize, which will allow the current attempt to
 * progress beyond the "sticky" edge.
 */
static struct {
	struct view *view;
	bool pending;
	int vertical_offset;
	int horizontal_offset;
	enum wlr_edges resize_edges;
	struct wlr_box geom;
} last_snap_hit;

static void
snap_constraints_reset(void)
{
	last_snap_hit.view = NULL;
	last_snap_hit.pending = false;
	last_snap_hit.horizontal_offset = INT_MIN;
	last_snap_hit.vertical_offset = INT_MIN;
	last_snap_hit.resize_edges = WLR_EDGE_NONE;
	memset(&last_snap_hit.geom, 0, sizeof(last_snap_hit.geom));
}

static bool
snap_constraints_are_valid(struct view *view, enum wlr_edges resize_edges)
{
	assert(view);

	/* Cache is not valid if view has changed */
	if (view != last_snap_hit.view) {
		return false;
	}

	/* Cache is not valid if expected edges do not match */
	if (resize_edges != last_snap_hit.resize_edges) {
		return false;
	}

	/* Cache is not valid if edge offsets are invalid */
	if (resize_edges & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) {
		if (!BOUNDED_INT(last_snap_hit.horizontal_offset)) {
			return false;
		}

		if ((resize_edges & WLR_EDGE_LEFT) && (resize_edges & WLR_EDGE_RIGHT)) {
			return false;
		}
	} else if (resize_edges & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)) {
		if (!BOUNDED_INT(last_snap_hit.vertical_offset)) {
			return false;
		}

		if ((resize_edges & WLR_EDGE_TOP) && (resize_edges & WLR_EDGE_BOTTOM)) {
			return false;
		}
	} else {
		return false;
	}

	/* Cache is valid iff view geometry matches expectation */
	return wlr_box_equal(&view->pending, &last_snap_hit.geom);
}

void
snap_constraints_set(struct view *view,
		enum wlr_edges resize_edges, struct wlr_box geom)
{
	assert(view);

	/* Set horizontal offset when resizing horizontal edges */
	last_snap_hit.horizontal_offset = INT_MIN;
	if (resize_edges & WLR_EDGE_LEFT) {
		last_snap_hit.horizontal_offset = geom.x;
	} else if (resize_edges & WLR_EDGE_RIGHT) {
		last_snap_hit.horizontal_offset = geom.x + geom.width;
	}

	/* Set vertical offset when resizing vertical edges */
	last_snap_hit.vertical_offset = INT_MIN;
	if (resize_edges & WLR_EDGE_TOP) {
		last_snap_hit.vertical_offset = geom.y;
	} else if (resize_edges & WLR_EDGE_BOTTOM) {
		last_snap_hit.vertical_offset = geom.y + geom.height;
	}

	/* Capture the current geometry and effective snapped edge */
	last_snap_hit.view = view;
	last_snap_hit.resize_edges = resize_edges;
	last_snap_hit.geom = geom;

	/*
	 * Client geometry change is pending, and XDG clients may adjust the
	 * geometry to match arbitrary constraints. Allow the client to update
	 * our concept of constraints exactly once after the configure request.
	 */
	last_snap_hit.pending = true;
}

void
snap_constraints_invalidate(struct view *view)
{
	if (view == last_snap_hit.view) {
		snap_constraints_reset();
	}
}

void
snap_constraints_update(struct view *view)
{
	assert(view);

	/* Ignore update if this is the wrong view */
	if (view != last_snap_hit.view) {
		return;
	}

	/* Never update constraints more than once */
	if (!last_snap_hit.pending) {
		return;
	}

	/* Only update constraints when pending view dimensions match expectation */
	if (view->pending.width != last_snap_hit.geom.width) {
		return;
	}

	if (view->pending.height != last_snap_hit.geom.height) {
		return;
	}

	last_snap_hit.geom = view->current;
	last_snap_hit.pending = false;
}

struct wlr_box
snap_constraints_effective(struct view *view,
		enum wlr_edges resize_edges, bool use_pending)
{
	assert(view);

	struct wlr_box real_geom = use_pending ? view->pending : view->current;

	/* Use actual geometry when constraints do not apply */
	if (!snap_constraints_are_valid(view, resize_edges)) {
		return real_geom;
	}

	/* Override changing edge with constrained value */
	struct wlr_box geom = real_geom;

	if (resize_edges & WLR_EDGE_LEFT) {
		geom.x = last_snap_hit.horizontal_offset;
	} else if (resize_edges & WLR_EDGE_RIGHT) {
		geom.width = last_snap_hit.horizontal_offset - geom.x;
	}

	if (resize_edges & WLR_EDGE_TOP) {
		geom.y = last_snap_hit.vertical_offset;
	} else if (resize_edges & WLR_EDGE_BOTTOM) {
		geom.height = last_snap_hit.vertical_offset - geom.y;
	}

	/* Fall back to actual geometry when constrained geometry is nonsense */
	if (!BOUNDED_INT(geom.x) || !BOUNDED_INT(geom.y)) {
		return real_geom;
	}

	if (geom.width <= 0 || geom.height <= 0) {
		return real_geom;
	}

	return geom;
}
