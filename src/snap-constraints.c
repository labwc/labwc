// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/util/box.h>
#include "common/macros.h"
#include "labwc.h"
#include "snap-constraints.h"
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
 * 1. The view is the same as in the last attemped snapped resize;
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
	int offset;
	enum view_edge direction;
	struct wlr_box geom;
} last_snap_hit;

static void
snap_constraints_reset(void)
{
	last_snap_hit.view = NULL;
	last_snap_hit.pending = false;
	last_snap_hit.offset = INT_MIN;
	last_snap_hit.direction = VIEW_EDGE_INVALID;
	memset(&last_snap_hit.geom, 0, sizeof(last_snap_hit.geom));
}

static bool
snap_constraints_are_valid(struct view *view, enum view_edge direction)
{
	assert(view);

	/* Cache is not valid if view has changed */
	if (view != last_snap_hit.view) {
		return false;
	}

	/* Cache is not valid if direction has changed */
	if (direction != last_snap_hit.direction) {
		return false;
	}

	/* Cache is not valid if offset is unbounded */
	if (!BOUNDED_INT(last_snap_hit.offset)) {
		return false;
	}

	/* Cache is valid iff pending view geometry matches expectation */
	return wlr_box_equal(&view->pending, &last_snap_hit.geom);
}

void
snap_constraints_set(struct view *view,
		enum view_edge direction, struct wlr_box geom)
{
	assert(view);

	int offset = INT_MIN;
	switch (direction) {
	case VIEW_EDGE_LEFT:
		offset = geom.x;
		break;
	case VIEW_EDGE_RIGHT:
		offset = geom.x + geom.width;
		break;
	case VIEW_EDGE_UP:
		offset = geom.y;
		break;
	case VIEW_EDGE_DOWN:
		offset = geom.y + geom.height;
		break;
	default:
		break;
	}

	if (!BOUNDED_INT(offset)) {
		snap_constraints_reset();
		return;
	}

	/* Capture the current geometry and effective snapped edge */
	last_snap_hit.view = view;
	last_snap_hit.offset = offset;
	last_snap_hit.direction = direction;
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

	/* Only update constraints when view geometry matches expectation */
	if (!wlr_box_equal(&view->pending, &last_snap_hit.geom)) {
		return;
	}

	last_snap_hit.geom = view->current;
	last_snap_hit.pending = false;
}

struct wlr_box
snap_constraints_effective(struct view *view, enum view_edge direction)
{
	assert(view);

	/* Use actual geometry when constraints do not apply */
	if (!snap_constraints_are_valid(view, direction)) {
		return view->pending;
	}

	/* Override changing edge with constrained value */
	struct wlr_box geom = view->pending;
	switch (last_snap_hit.direction) {
	case VIEW_EDGE_LEFT:
		geom.x = last_snap_hit.offset;
		break;
	case VIEW_EDGE_RIGHT:
		geom.width = last_snap_hit.offset - geom.x;
		break;
	case VIEW_EDGE_UP:
		geom.y = last_snap_hit.offset;
		break;
	case VIEW_EDGE_DOWN:
		geom.height = last_snap_hit.offset - geom.y;
		break;
	default:
		return view->pending;
	}

	/* Fall back to actual geometry when constrained geometry is nonsense */
	if (geom.width <= 0 || geom.height <= 0) {
		return view->pending;
	}

	return geom;
}
