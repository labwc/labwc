// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "regions.h"
#include "view.h"

bool
regions_should_snap(struct server *server)
{
	if (server->input_mode != LAB_INPUT_STATE_MOVE
			|| wl_list_empty(&rc.regions)
			|| server->seat.region_prevent_snap) {
		return false;
	}

	struct wlr_keyboard *keyboard = &server->seat.keyboard_group->keyboard;
	return keyboard_any_modifiers_pressed(keyboard);
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
regions_reconfigure_output(struct output *output)
{
	assert(output);

	/* Evacuate views and destroy current regions */
	if (!wl_list_empty(&output->regions)) {
		regions_evacuate_output(output);
		regions_destroy(&output->server->seat, &output->regions);
	}

	/* Initialize regions from config */
	struct region *region;
	wl_list_for_each(region, &rc.regions, link) {
		struct region *region_new = znew(*region_new);
		/* Create a copy */
		region_new->output = output;
		region_new->name = xstrdup(region->name);
		region_new->percentage = region->percentage;
		wl_list_append(&output->regions, &region_new->link);
	}

	/* Update region geometries */
	regions_update_geometry(output);
}

void
regions_reconfigure(struct server *server)
{
	struct output *output;

	/* Evacuate views and initialize regions from config */
	wl_list_for_each(output, &server->outputs, link) {
		regions_reconfigure_output(output);
	}

	/* Tries to match the evacuated views to the new regions */
	desktop_arrange_all_views(server);
}

void
regions_update_geometry(struct output *output)
{
	assert(output);

	struct region *region;
	struct wlr_box usable = output_usable_area_in_layout_coords(output);

	/* Update regions */
	struct wlr_box *perc, *geo;
	wl_list_for_each(region, &output->regions, link) {
		geo = &region->geo;
		perc = &region->percentage;
		/*
		 * Add percentages (x + width, y + height) before scaling
		 * so that there is no gap between regions due to rounding
		 * variations
		 */
		int left = usable.width * perc->x / 100;
		int right = usable.width * (perc->x + perc->width) / 100;
		int top = usable.height * perc->y / 100;
		int bottom = usable.height * (perc->y + perc->height) / 100;
		geo->x = usable.x + left;
		geo->y = usable.y + top;
		geo->width = right - left;
		geo->height = bottom - top;
		region->center.x = geo->x + geo->width / 2;
		region->center.y = geo->y + geo->height / 2;
	}
}

void
regions_evacuate_output(struct output *output)
{
	assert(output);
	struct view *view;
	wl_list_for_each(view, &output->server->views, link) {
		if (view->tiled_region && view->tiled_region->output == output) {
			view_evacuate_region(view);
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
		if (seat && seat->overlay.active.region == region) {
			overlay_hide(seat);
		}
		zfree(region->name);
		zfree(region);
	}
}

