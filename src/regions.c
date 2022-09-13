// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <wlr/render/pixman.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/graphic-helpers.h"
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "regions.h"
#include "view.h"

bool
regions_available(void)
{
	return !wl_list_empty(&rc.regions);
}

static void
overlay_create(struct seat *seat)
{
	assert(!seat->region_overlay.tree);

	struct server *server = seat->server;
	struct wlr_scene_tree *parent = wlr_scene_tree_create(&server->scene->tree);

	seat->region_overlay.tree = parent;
	wlr_scene_node_set_enabled(&parent->node, false);
	if (!wlr_renderer_is_pixman(server->renderer)) {
		/* Hardware assisted rendering: Half transparent overlay */
		float color[4] = { 0.25, 0.25, 0.35, 0.5 };
		seat->region_overlay.overlay = wlr_scene_rect_create(parent, 0, 0, color);
	} else {
		/* Software rendering: Outlines */
		int line_width = server->theme->osd_border_width;
		float *colors[3] = {
			server->theme->osd_bg_color,
			server->theme->osd_label_text_color,
			server->theme->osd_bg_color
		};
		seat->region_overlay.pixman_overlay = multi_rect_create(parent, colors, line_width);
	}
}

struct region *
regions_from_name(const char *region_name, struct output *output)
{
	assert(region_name);
	assert(output);
	struct region *region;
	wl_list_for_each(region, &output->regions, link) {
		if (!strcmp(region->name, region_name)) {
			return region;
		}
	}
	return NULL;
}

struct region *
regions_from_cursor(struct server *server)
{
	assert(server);
	double lx = server->seat.cursor->x;
	double ly = server->seat.cursor->y;

	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout, lx, ly);
	struct output *output = output_from_wlr_output(server, wlr_output);
	if (!output) {
		return NULL;
	}

	double dist;
	double dist_min = DBL_MAX;
	struct region *closest_region = NULL;
	struct region *region;
	wl_list_for_each(region, &output->regions, link) {
		if (wlr_box_contains_point(&region->geo, lx, ly)) {
			/* No need for sqrt((x1 - x2)^2 + (y1 - y2)^2) as we just compare */
			dist = pow(region->center.x - lx, 2) + pow(region->center.y - ly, 2);
			if (dist < dist_min) {
				closest_region = region;
				dist_min = dist;
			}
		}
	}
	return closest_region;
}

void
regions_show_overlay(struct view *view, struct seat *seat, struct region *region)
{
	assert(view);
	assert(seat);
	assert(region);

	/* Don't show active region */
	if (seat->region_active == region) {
		return;
	}

	if (!seat->region_overlay.tree) {
		overlay_create(seat);
	}

	/* Update overlay */
	struct server *server = seat->server;
	struct wlr_scene_node *node = &seat->region_overlay.tree->node;
	if (!wlr_renderer_is_pixman(server->renderer)) {
		/* Hardware assisted rendering: Half transparent overlay */
		wlr_scene_rect_set_size(seat->region_overlay.overlay,
			region->geo.width, region->geo.height);
	} else {
		/* Software rendering: Outlines */
		multi_rect_set_size(seat->region_overlay.pixman_overlay,
			region->geo.width, region->geo.height);
	}
	if (node->parent != view->scene_tree->node.parent) {
		wlr_scene_node_reparent(node, view->scene_tree->node.parent);
		wlr_scene_node_place_below(node, &view->scene_tree->node);
	}
	wlr_scene_node_set_position(node, region->geo.x, region->geo.y);
	wlr_scene_node_set_enabled(node, true);
	seat->region_active = region;
}

void
regions_hide_overlay(struct seat *seat)
{
	assert(seat);
	if (!seat->region_active) {
		return;
	}

	struct server *server = seat->server;
	struct wlr_scene_node *node = &seat->region_overlay.tree->node;

	wlr_scene_node_set_enabled(node, false);
	if (node->parent != &server->scene->tree) {
		wlr_scene_node_reparent(node, &server->scene->tree);
	}
	seat->region_active = NULL;
}

void
regions_update(struct output *output)
{
	assert(output);

	struct region *region;
	struct wlr_box usable = output_usable_area_in_layout_coords(output);

	/* Initialize regions */
	if (wl_list_empty(&output->regions)) {
		wl_list_for_each(region, &rc.regions, link) {
			struct region *region_new = znew(*region_new);
			/* Create a copy */
			region_new->name = xstrdup(region->name);
			region_new->percentage = region->percentage;
			wl_list_append(&output->regions, &region_new->link);
		}
	}

	if (wlr_box_equal(&output->regions_usable_area, &usable)) {
		/* Nothing changed */
		return;
	}
	output->regions_usable_area = usable;

	/* Update regions */
	struct wlr_box *perc, *geo;
	wl_list_for_each(region, &output->regions, link) {
		geo = &region->geo;
		perc = &region->percentage;
		geo->x = usable.x + usable.width * perc->x / 100;
		geo->y = usable.y + usable.height * perc->y / 100;
		geo->width = usable.width * perc->width / 100;
		geo->height = usable.height * perc->height / 100;
		region->center.x = geo->x + geo->width / 2;
		region->center.y = geo->y + geo->height / 2;
	}
}

void
regions_evacuate_output(struct output *output)
{
	assert(output);
	struct view *view;
	struct region *region;

	wl_list_for_each(view, &output->server->views, link) {
		wl_list_for_each(region, &output->regions, link) {
			if (view->tiled_region == region) {
				if (!view->tiled_region_evacuate) {
					view->tiled_region_evacuate =
						xstrdup(region->name);
				}
				break;
			}
		}
	}
}

void
regions_destroy(struct seat *seat, struct wl_list *regions)
{
	assert(regions);
	struct region *region, *region_tmp;
	wl_list_for_each_safe(region, region_tmp, regions, link) {
		wl_list_remove(&region->link);
		zfree(region->name);
		if (seat && seat->region_active == region) {
			seat->region_active = NULL;
		}
		zfree(region);
	}
}

