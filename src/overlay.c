// SPDX-License-Identifier: GPL-2.0-only
#include "overlay.h"
#include <assert.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include "common/lab-scene-rect.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"
#include "regions.h"
#include "theme.h"
#include "view.h"

static void
show_overlay(struct seat *seat, struct theme_snapping_overlay *overlay_theme,
		struct wlr_box *box)
{
	struct server *server = seat->server;
	struct view *view = server->grabbed_view;
	assert(view);
	assert(!seat->overlay.rect);

	struct lab_scene_rect_options opts = {
		.width = box->width,
		.height = box->height,
	};
	if (overlay_theme->bg_enabled) {
		/* Create a filled rectangle */
		opts.bg_color = overlay_theme->bg_color;
	}
	float *border_colors[3] = {
		overlay_theme->border_color[0],
		overlay_theme->border_color[1],
		overlay_theme->border_color[2],
	};
	if (overlay_theme->border_enabled) {
		/* Create outlines */
		opts.border_colors = border_colors;
		opts.nr_borders = 3;
		opts.border_width = overlay_theme->border_width;
	}

	seat->overlay.rect =
		lab_scene_rect_create(view->scene_tree->node.parent, &opts);

	struct wlr_scene_node *node = &seat->overlay.rect->tree->node;
	wlr_scene_node_place_below(node, &view->scene_tree->node);
	wlr_scene_node_set_position(node, box->x, box->y);
}

static void
show_region_overlay(struct seat *seat, struct region *region)
{
	if (region == seat->overlay.active.region) {
		return;
	}
	overlay_finish(seat);
	seat->overlay.active.region = region;

	struct wlr_box geo = view_get_region_snap_box(NULL, region);
	show_overlay(seat, &rc.theme->snapping_overlay_region, &geo);
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
	show_overlay(seat, &rc.theme->snapping_overlay_edge, &box);
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
	overlay_finish(seat);
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
		handle_edge_overlay_timeout(seat);
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

	overlay_finish(seat);
}

void
overlay_finish(struct seat *seat)
{
	if (seat->overlay.rect) {
		wlr_scene_node_destroy(&seat->overlay.rect->tree->node);
	}
	if (seat->overlay.timer) {
		wl_event_source_remove(seat->overlay.timer);
	}
	seat->overlay = (struct overlay){0};
}
