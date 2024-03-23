// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/render/pixman.h>
#include "labwc.h"
#include "overlay.h"
#include "view.h"

static void
create_overlay(struct seat *seat)
{
	assert(!seat->overlay.tree);

	struct server *server = seat->server;
	struct wlr_scene_tree *parent = wlr_scene_tree_create(&server->scene->tree);

	seat->overlay.tree = parent;
	wlr_scene_node_set_enabled(&parent->node, false);
	if (!wlr_renderer_is_pixman(server->renderer)) {
		/* Hardware assisted rendering: Half transparent overlay */
		float color[4] = { 0.25, 0.25, 0.35, 0.5 };
		seat->overlay.rect = wlr_scene_rect_create(parent, 0, 0, color);
	} else {
		/* Software rendering: Outlines */
		int line_width = server->theme->osd_border_width;
		float *colors[3] = {
			server->theme->osd_bg_color,
			server->theme->osd_label_text_color,
			server->theme->osd_bg_color
		};
		seat->overlay.pixman_rect = multi_rect_create(parent, colors, line_width);
	}
}

static void
cancel_pending_overlay(struct overlay *overlay)
{
	if (!overlay->timer) {
		return;
	}
	wl_event_source_remove(overlay->timer);
	overlay->timer = NULL;
	overlay->view = NULL;
	overlay->box = (struct wlr_box){0};
}

static void
show_overlay(struct seat *seat, struct view *view, struct wlr_box *box)
{
	if (!seat->overlay.tree) {
		create_overlay(seat);
	}

	/* Update overlay */
	struct server *server = seat->server;
	struct wlr_scene_node *node = &seat->overlay.tree->node;
	if (!wlr_renderer_is_pixman(server->renderer)) {
		/* Hardware assisted rendering: Half transparent overlay */
		wlr_scene_rect_set_size(seat->overlay.rect,
			box->width, box->height);
	} else {
		/* Software rendering: Outlines */
		multi_rect_set_size(seat->overlay.pixman_rect,
			box->width, box->height);
	}
	if (node->parent != view->scene_tree->node.parent) {
		wlr_scene_node_reparent(node, view->scene_tree->node.parent);
		wlr_scene_node_place_below(node, &view->scene_tree->node);
	}
	wlr_scene_node_set_position(node, box->x, box->y);
	wlr_scene_node_set_enabled(node, true);

	cancel_pending_overlay(&seat->overlay);
}

static int
handle_overlay_timeout(void *data)
{
	struct seat *seat = data;
	show_overlay(seat, seat->overlay.view, &seat->overlay.box);
	return 0;
}

static void
show_overlay_delayed(struct seat *seat, struct view *view, struct wlr_box *box)
{
	if (seat->overlay.timer
			&& seat->overlay.view == view
			&& wlr_box_equal(box, &seat->overlay.box)) {
		return;
	}
	cancel_pending_overlay(&seat->overlay);
	seat->overlay.view = view;
	seat->overlay.box = *box;
	seat->overlay.timer = wl_event_loop_add_timer(
		seat->server->wl_event_loop,
		handle_overlay_timeout, seat);
	wl_event_source_timer_update(seat->overlay.timer, 150);
}

void
overlay_show(struct seat *seat, struct view *view)
{
	struct server *server = seat->server;

	/*
	 * TODO: cache return value of regions_from_cursor() or
	 * edge_from_cursor() to eliminate duplicated calls to show_overlay() /
	 * show_overlay_delayed().
	 */
	if (regions_should_snap(server)) {
		/* Region overlay */
		struct region *region = regions_from_cursor(server);
		if (region) {
			show_overlay(seat, view, &region->geo);
			return;
		}
	} else {
		/* Snap-to-edge overlay */
		struct output *output;
		enum view_edge edge = edge_from_cursor(seat, &output);
		if (edge != VIEW_EDGE_INVALID) {
			/* TODO: share logic with view_get_edge_snap_box() */
			struct wlr_box box =
				output_usable_area_in_layout_coords(output);
			switch (edge) {
			case VIEW_EDGE_RIGHT:
				box.x += box.width / 2;
				/* fallthrough */
			case VIEW_EDGE_LEFT:
				box.width /= 2;
				break;
			case VIEW_EDGE_DOWN:
				box.y += box.height / 2;
				/* fallthrough */
			case VIEW_EDGE_UP:
				box.height /= 2;
				break;
			case VIEW_EDGE_CENTER:
				/* <topMaximize> */
				break;
			case VIEW_EDGE_INVALID:
				/* not reached */
				assert(false);
			}
			/*
			 * Delay showing overlay to prevent flickering when
			 * dragging view across output edges in multi-monitor
			 * setup
			 */
			show_overlay_delayed(seat, view, &box);
			return;
		}
	}
	overlay_hide(seat);
}

void
overlay_hide(struct seat *seat)
{
	cancel_pending_overlay(&seat->overlay);

	struct server *server = seat->server;
	struct wlr_scene_node *node = &seat->overlay.tree->node;
	if (!node) {
		return;
	}

	wlr_scene_node_set_enabled(node, false);
	if (node->parent != &server->scene->tree) {
		wlr_scene_node_reparent(node, &server->scene->tree);
	}
}
