// SPDX-License-Identifier: GPL-2.0-only
/* view-impl-common.c: common code for shell view->impl functions */
#include "view-impl-common.h"
#include "labwc.h"
#include "view.h"

static bool
resizing_edge(struct view *view, enum lab_edge edge)
{
	struct server *server = view->server;
	return server->input_mode == LAB_INPUT_STATE_RESIZE
		&& server->grabbed_view == view
		&& (server->resize_edges & edge);
}

void
view_impl_apply_geometry(struct view *view, int w, int h)
{
	struct wlr_box *current = &view->current;
	struct wlr_box *pending = &view->pending;
	struct wlr_box old = *current;

	/*
	 * Anchor right edge if resizing via left edge.
	 *
	 * Note that answering the question "are we resizing?" is a bit
	 * tricky. The most obvious method is to look at the server
	 * flags; but that method will not account for any late commits
	 * that occur after the mouse button is released, as the client
	 * catches up with pending configure requests. So as a fallback,
	 * we resort to a geometry-based heuristic -- also not 100%
	 * reliable on its own. The combination of the two methods
	 * should catch 99% of resize cases that we care about.
	 */
	bool resizing_left_edge = resizing_edge(view, LAB_EDGE_LEFT);
	if (resizing_left_edge || (current->x != pending->x
			&& current->x + current->width ==
			pending->x + pending->width)) {
		current->x = pending->x + pending->width - w;
	} else {
		current->x = pending->x;
	}

	/* Anchor bottom edge if resizing via top edge */
	bool resizing_top_edge = resizing_edge(view, LAB_EDGE_TOP);
	if (resizing_top_edge || (current->y != pending->y
			&& current->y + current->height ==
			pending->y + pending->height)) {
		current->y = pending->y + pending->height - h;
	} else {
		current->y = pending->y;
	}

	current->width = w;
	current->height = h;

	if (!wlr_box_equal(current, &old)) {
		view_moved(view);
	}
}
