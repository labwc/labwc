// SPDX-License-Identifier: GPL-2.0-only
/* view-impl-common.c: common code for shell view->impl functions */
#include <stdio.h>
#include <strings.h>
#include "common/list.h"
#include "foreign-toplevel.h"
#include "labwc.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"

void
view_impl_move_to_front(struct view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
view_impl_move_to_back(struct view *view)
{
	wl_list_remove(&view->link);
	wl_list_append(&view->server->views, &view->link);
	wlr_scene_node_lower_to_bottom(&view->scene_tree->node);
}

void
view_impl_map(struct view *view)
{
	desktop_focus_view(view, /*raise*/ true);
	view_update_title(view);
	view_update_app_id(view);
	if (!view->been_mapped) {
		window_rules_apply(view, LAB_WINDOW_RULE_EVENT_ON_FIRST_MAP);
	}

	/*
	 * It's tempting to just never create the foreign-toplevel handle in the
	 * map handlers, but the app_id/title might not have been set at that
	 * point, so it's safer to process the property here
	 */
	enum property ret = window_rules_get_property(view, "skipTaskbar");
	if (ret == LAB_PROP_TRUE) {
		if (view->foreign_toplevel) {
			foreign_toplevel_destroy(view->foreign_toplevel);
			view->foreign_toplevel = NULL;
		}
	}

	/*
	 * Some clients (e.g. Steam's Big Picture Mode window) request
	 * fullscreen before mapping.
	 */
	desktop_update_top_layer_visibility(view->server);

	wlr_log(WLR_DEBUG, "[map] identifier=%s, title=%s",
		view_get_string_prop(view, "app_id"),
		view_get_string_prop(view, "title"));
}

void
view_impl_unmap(struct view *view)
{
	struct server *server = view->server;
	if (view == server->active_view) {
		desktop_focus_topmost_view(server);
	}
	if (view == server->last_raised_view) {
		server->last_raised_view = NULL;
	}
}

static bool
resizing_edge(struct view *view, uint32_t edge)
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
	bool resizing_left_edge = resizing_edge(view, WLR_EDGE_LEFT);
	if (resizing_left_edge || (current->x != pending->x
			&& current->x + current->width ==
			pending->x + pending->width)) {
		current->x = pending->x + pending->width - w;
	} else {
		current->x = pending->x;
	}

	/* Anchor bottom edge if resizing via top edge */
	bool resizing_top_edge = resizing_edge(view, WLR_EDGE_TOP);
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
