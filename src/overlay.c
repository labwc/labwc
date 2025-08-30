// SPDX-License-Identifier: GPL-2.0-only
#include "overlay.h"
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "common/lab-scene-rect.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"
#include "regions.h"
#include "theme.h"
#include "view.h"

static void
create_overlay_rect(struct seat *seat, struct overlay_rect *rect,
		struct theme_snapping_overlay *theme)
{
	struct server *server = seat->server;

	rect->bg_enabled = theme->bg_enabled;
	rect->border_enabled = theme->border_enabled;
	rect->tree = wlr_scene_tree_create(&server->scene->tree);

	if (rect->bg_enabled) {
		/* Create a filled rectangle */
		rect->bg_rect = wlr_scene_rect_create(
			rect->tree, 0, 0, theme->bg_color);
	}

	if (rect->border_enabled) {
		/* Create outlines */
		struct lab_scene_rect_options opts = {
			.border_colors = (float *[3]) {
				theme->border_color[0],
				theme->border_color[1],
				theme->border_color[2],
			},
			.nr_borders = 3,
			.border_width = theme->border_width,
		};
		rect->border_rect = lab_scene_rect_create(rect->tree, &opts);
	}

	wlr_scene_node_set_enabled(&rect->tree->node, false);
}

void overlay_reconfigure(struct seat *seat)
{
	if (seat->overlay.region_rect.tree) {
		wlr_scene_node_destroy(&seat->overlay.region_rect.tree->node);
	}
	if (seat->overlay.edge_rect.tree) {
		wlr_scene_node_destroy(&seat->overlay.edge_rect.tree->node);
	}

	struct theme *theme = seat->server->theme;
	create_overlay_rect(seat, &seat->overlay.region_rect,
		&theme->snapping_overlay_region);
	create_overlay_rect(seat, &seat->overlay.edge_rect,
		&theme->snapping_overlay_edge);
}

static void
show_overlay(struct seat *seat, struct overlay_rect *rect, struct wlr_box *box)
{
	struct server *server = seat->server;
	struct view *view = server->grabbed_view;
	assert(view);

	if (!rect->tree) {
		overlay_reconfigure(seat);
		assert(rect->tree);
	}

	if (rect->bg_enabled) {
		wlr_scene_rect_set_size(rect->bg_rect, box->width, box->height);
	}
	if (rect->border_enabled) {
		lab_scene_rect_set_size(rect->border_rect, box->width, box->height);
	}

	struct wlr_scene_node *node = &rect->tree->node;
	wlr_scene_node_reparent(node, view->scene_tree->node.parent);
	wlr_scene_node_place_below(node, &view->scene_tree->node);
	wlr_scene_node_set_position(node, box->x, box->y);
	wlr_scene_node_set_enabled(node, true);
}

static void
inactivate_overlay(struct overlay *overlay)
{
	if (overlay->region_rect.tree) {
		wlr_scene_node_set_enabled(
			&overlay->region_rect.tree->node, false);
	}
	if (overlay->edge_rect.tree) {
		wlr_scene_node_set_enabled(
			&overlay->edge_rect.tree->node, false);
	}
	overlay->active.region = NULL;
	overlay->active.edge = LAB_EDGE_NONE;
	overlay->active.output = NULL;
	if (overlay->timer) {
		wl_event_source_timer_update(overlay->timer, 0);
	}
}

static void
show_region_overlay(struct seat *seat, struct region *region)
{
	if (region == seat->overlay.active.region) {
		return;
	}
	inactivate_overlay(&seat->overlay);
	seat->overlay.active.region = region;

	struct wlr_box geo = view_get_region_snap_box(NULL, region);
	show_overlay(seat, &seat->overlay.region_rect, &geo);
}

static struct wlr_box
get_edge_snap_box(enum lab_edge edge, struct output *output)
{
	if (edge == LAB_EDGE_TOP && rc.snap_top_maximize) {
		return output_usable_area_in_layout_coords(output);
	} else {
		return view_get_edge_snap_box(NULL, output, edge);
	}
}

static int
handle_edge_overlay_timeout(void *data)
{
	struct seat *seat = data;
	assert(seat->overlay.active.edge != LAB_EDGE_NONE
		&& seat->overlay.active.output);
	struct wlr_box box = get_edge_snap_box(seat->overlay.active.edge,
		seat->overlay.active.output);
	show_overlay(seat, &seat->overlay.edge_rect, &box);
	return 0;
}

static bool
edge_has_adjacent_output_from_cursor(struct seat *seat, struct output *output,
		enum lab_edge edge)
{
	/* Allow only up/down/left/right */
	if (!lab_edge_is_cardinal(edge)) {
		return false;
	}
	/* Cast from enum lab_edge to enum wlr_direction is safe */
	return wlr_output_layout_adjacent_output(
		seat->server->output_layout, (enum wlr_direction)edge,
		output->wlr_output, seat->cursor->x, seat->cursor->y);
}

static void
show_edge_overlay(struct seat *seat, enum lab_edge edge1, enum lab_edge edge2,
		struct output *output)
{
	if (!rc.snap_overlay_enabled) {
		return;
	}
	enum lab_edge edge = edge1 | edge2;
	if (seat->overlay.active.edge == edge
			&& seat->overlay.active.output == output) {
		return;
	}
	inactivate_overlay(&seat->overlay);
	seat->overlay.active.edge = edge;
	seat->overlay.active.output = output;

	int delay;
	if (edge_has_adjacent_output_from_cursor(seat, output, edge1)) {
		delay = rc.snap_overlay_delay_inner;
	} else {
		delay = rc.snap_overlay_delay_outer;
	}

	if (delay > 0) {
		if (!seat->overlay.timer) {
			seat->overlay.timer = wl_event_loop_add_timer(
				seat->server->wl_event_loop,
				handle_edge_overlay_timeout, seat);
		}
		/* Show overlay <snapping><preview><delay>ms later */
		wl_event_source_timer_update(seat->overlay.timer, delay);
	} else {
		/* Show overlay now */
		struct wlr_box box = get_edge_snap_box(edge, output);
		show_overlay(seat, &seat->overlay.edge_rect, &box);
	}
}

void
overlay_update(struct seat *seat)
{
	struct server *server = seat->server;

	/* Region-snapping overlay */
	if (regions_should_snap(server)) {
		struct region *region = regions_from_cursor(server);
		if (region) {
			show_region_overlay(seat, region);
			return;
		}
	}

	/* Edge-snapping overlay */
	struct output *output;
	enum lab_edge edge1, edge2;
	if (edge_from_cursor(seat, &output, &edge1, &edge2)) {
		show_edge_overlay(seat, edge1, edge2, output);
		return;
	}

	overlay_hide(seat);
}

void
overlay_hide(struct seat *seat)
{
	struct overlay *overlay = &seat->overlay;
	struct server *server = seat->server;

	inactivate_overlay(overlay);

	/*
	 * Reparent the rectangle nodes to server's scene-tree so they don't
	 * get destroyed on view destruction
	 */
	if (overlay->region_rect.tree) {
		wlr_scene_node_reparent(&overlay->region_rect.tree->node,
			&server->scene->tree);
	}
	if (overlay->edge_rect.tree) {
		wlr_scene_node_reparent(&overlay->edge_rect.tree->node,
			&server->scene->tree);
	}
}

void
overlay_finish(struct seat *seat)
{
	if (seat->overlay.timer) {
		wl_event_source_remove(seat->overlay.timer);
		seat->overlay.timer = NULL;
	}
}
