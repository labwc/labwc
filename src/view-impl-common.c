// SPDX-License-Identifier: GPL-2.0-only
/* view-impl-common.c: common code for shell view->impl functions */
#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "menu/menu.h"
#include "node.h"
#include "ssd.h"
#include "view.h"
#include "view-impl-common.h"
#include "workspaces.h"

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
	desktop_focus_and_activate_view(&view->server->seat, view);
	view_move_to_front(view);
	view_update_title(view);
	view_update_app_id(view);
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

void
view_impl_remove_common_listeners(struct view *view)
{
	/* Events shared by view implementations */
	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_minimize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	wl_list_remove(&view->set_title.link);
}

void
view_init(struct view *view)
{
	assert(view);

	view->workspace = view->server->workspace_current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	node_descriptor_create(&view->scene_tree->node,
		LAB_NODE_DESC_VIEW, view);

	view->impl->listeners_init(view);

	wl_list_insert(&view->server->views, &view->link);
}

void
view_destroy(struct view *view)
{
	assert(view);
	struct server *server = view->server;
	bool need_cursor_update = false;

	view->impl->listeners_remove(view);

	if (view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_destroy(view->toplevel.handle);
	}

	if (server->grabbed_view == view) {
		/* Application got killed while moving around */
		server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
		server->grabbed_view = NULL;
		need_cursor_update = true;
		regions_hide_overlay(&server->seat);
	}

	if (server->focused_view == view) {
		server->focused_view = NULL;
		need_cursor_update = true;
	}

	if (server->seat.pressed.view == view) {
		seat_reset_pressed(&server->seat);
	}

	if (view->tiled_region_evacuate) {
		zfree(view->tiled_region_evacuate);
	}

	osd_on_view_destroy(view);
	ssd_destroy(view->ssd);
	view->ssd = NULL;

	if (view->scene_tree) {
		wlr_scene_node_destroy(&view->scene_tree->node);
		view->scene_tree = NULL;
	}

	/*
	 * The layer-shell top-layer is disabled when an application is running
	 * in fullscreen mode, so if that's the case, we have to re-enable it
	 * here.
	 */
	if (view->fullscreen && view->output) {
		uint32_t top = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
		wlr_scene_node_set_enabled(&view->output->layer_tree[top]->node,
			true);
	}

	/* If we spawned a window menu, close it */
	if (server->menu_current
			&& server->menu_current->triggered_by_view == view) {
		menu_close_root(server);
	}

	/* Remove view from server->views */
	wl_list_remove(&view->link);
	free(view);

	if (need_cursor_update) {
		cursor_update_focus(server);
	}
}
