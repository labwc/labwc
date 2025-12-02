// SPDX-License-Identifier: GPL-2.0-only
/* view-impl-common.c: common code for shell view->impl functions */
#include "view-impl-common.h"
#include "foreign-toplevel/foreign.h"
#include "labwc.h"
#include "view.h"
#include "window-rules.h"

void
view_impl_map(struct view *view)
{
	view_update_visibility(view);

	if (!view->been_mapped) {
		window_rules_apply(view, LAB_WINDOW_RULE_EVENT_ON_FIRST_MAP);
	}

	/*
	 * Create foreign-toplevel handle, respecting skipTaskbar rules.
	 * Also exclude unfocusable views (popups, floating toolbars,
	 * etc.) as these should not be shown in taskbars/docks/etc.
	 */
	if (!view->foreign_toplevel && view_is_focusable(view)
			&& window_rules_get_property(view, "skipTaskbar")
				!= LAB_PROP_TRUE) {
		view->foreign_toplevel = foreign_toplevel_create(view);

		struct view *parent = view->impl->get_parent(view);
		if (parent && parent->foreign_toplevel) {
			foreign_toplevel_set_parent(view->foreign_toplevel,
				parent->foreign_toplevel);
		}
	}

	wlr_log(WLR_DEBUG, "[map] identifier=%s, title=%s",
		view->app_id, view->title);
}

void
view_impl_unmap(struct view *view)
{
	view_update_visibility(view);

	/*
	 * Destroy the foreign toplevel handle so the unmapped view
	 * doesn't show up in panels and the like.
	 */
	if (view->foreign_toplevel) {
		foreign_toplevel_destroy(view->foreign_toplevel);
		view->foreign_toplevel = NULL;
	}
}

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
}
