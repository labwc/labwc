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
show_overlay(struct seat *seat, struct wlr_box *box)
{
	struct server *server = seat->server;
	struct view *view = server->grabbed_view;
	assert(view);

	if (!seat->overlay.tree) {
		create_overlay(seat);
	}

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

	if (seat->overlay.timer) {
		wl_event_source_timer_update(seat->overlay.timer, 0);
	}
}

static void
show_region_overlay(struct seat *seat, struct region *region)
{
	if (region == seat->overlay.active.region) {
		return;
	}
	seat->overlay.active.region = region;
	seat->overlay.active.edge = VIEW_EDGE_INVALID;
	seat->overlay.active.output = NULL;

	show_overlay(seat, &region->geo);
}

/* TODO: share logic with view_get_edge_snap_box() */
static struct wlr_box get_edge_snap_box(enum view_edge edge, struct output *output)
{
	struct wlr_box box = output_usable_area_in_layout_coords(output);
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
	default:
		/* not reached */
		assert(false);
	}
	return box;
}

static int
handle_edge_overlay_timeout(void *data)
{
	struct seat *seat = data;
	assert(seat->overlay.active.edge != VIEW_EDGE_INVALID
		&& seat->overlay.active.output);
	struct wlr_box box = get_edge_snap_box(seat->overlay.active.edge,
		seat->overlay.active.output);
	show_overlay(seat, &box);
	return 0;
}

static void
show_edge_overlay_delayed(struct seat *seat, enum view_edge edge,
		struct output *output)
{
	if (seat->overlay.active.edge == edge
			&& seat->overlay.active.output == output) {
		return;
	}
	seat->overlay.active.region = NULL;
	seat->overlay.active.edge = edge;
	seat->overlay.active.output = output;

	if (!seat->overlay.timer) {
		seat->overlay.timer = wl_event_loop_add_timer(
			seat->server->wl_event_loop,
			handle_edge_overlay_timeout, seat);
	}
	/* Delay for 150ms */
	wl_event_source_timer_update(seat->overlay.timer, 150);
}

void
overlay_show(struct seat *seat)
{
	struct server *server = seat->server;

	/* Region overlay */
	if (regions_should_snap(server)) {
		struct region *region = regions_from_cursor(server);
		if (region) {
			show_region_overlay(seat, region);
			return;
		}
	}

	/* Snap-to-edge overlay */
	struct output *output;
	enum view_edge edge = edge_from_cursor(seat, &output);
	if (edge != VIEW_EDGE_INVALID) {
		/*
		 * Snap-to-edge overlay is delayed for 150ms to prevent
		 * flickering when dragging view across output edges in
		 * multi-monitor setup.
		 */
		show_edge_overlay_delayed(seat, edge, output);
		return;
	}

	overlay_hide(seat);
}

void
overlay_hide(struct seat *seat)
{
	seat->overlay.active.region = NULL;
	seat->overlay.active.edge = VIEW_EDGE_INVALID;
	seat->overlay.active.output = NULL;
	if (seat->overlay.timer) {
		wl_event_source_timer_update(seat->overlay.timer, 0);
	}

	if (!seat->overlay.tree) {
		return;
	}
	struct server *server = seat->server;
	struct wlr_scene_node *node = &seat->overlay.tree->node;

	wlr_scene_node_set_enabled(node, false);
	if (node->parent != &server->scene->tree) {
		wlr_scene_node_reparent(node, &server->scene->tree);
	}
}