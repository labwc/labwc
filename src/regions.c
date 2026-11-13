// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include "regions.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include "common/lab-scene-rect.h"
#include "common/list.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "theme.h"
#include "view.h"

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
regions_from_cursor(void)
{
	if (server.input_mode != LAB_INPUT_STATE_MOVE
			|| wl_list_empty(&rc.regions)
			|| server.seat.region_prevent_snap
			|| !view_is_floating(server.grabbed_view)) {
		return NULL;
	}
	uint32_t modifiers = keyboard_get_all_modifiers(&server.seat);
	if (!modifiers && !rc.regions_drop_indicator) {
		return NULL;
	}
	double lx = server.seat.cursor->x;
	double ly = server.seat.cursor->y;

	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server.output_layout, lx, ly);
	struct output *output = output_from_wlr_output(wlr_output);
	if (!output_is_usable(output)) {
		return NULL;
	}

	if (rc.regions_drop_indicator) {
		struct wlr_scene_node *node = wlr_scene_node_at(
			&output->region_drop_indicator_tree->node, lx, ly, NULL, NULL);
		if (node) {
			while (node) {
				if (node->parent == output->region_drop_indicator_tree) {
					struct node_descriptor *desc = node->data;
					if (desc->type == LAB_NODE_REGION_DROP_INDICATOR) {
						return desc->data;
					}
				}
				if (!node->parent) {
					break;
				}
				node = &node->parent->node;
			}
		}
	}

	/* Check again in case "regions_show_indicator" is true but didn't find a region */
	if (!modifiers) {
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

static void
create_region_drop_indicators(struct output *output)
{
	if (!rc.regions_drop_indicator) {
		return;
	}
	struct region *region;
	wl_list_for_each(region, &output->regions, link) {
		struct drop_indicator_theme *theme = &rc.theme->region_drop_indicator;
		struct wlr_scene_tree *tree = output->region_drop_indicator_tree;
		struct lab_scene_rect_options opts = {
			.border_colors = (float *[1]) { theme->border_color },
			.nr_borders = 1,
			.border_width = theme->border_width,
			.bg_color = theme->bg_color,
			.width = theme->width,
			.height = theme->height,
		};
		struct lab_scene_rect *rect = lab_scene_rect_create(tree, &opts);
		node_descriptor_create(&rect->tree->node, LAB_NODE_REGION_DROP_INDICATOR,
			NULL, region);
		int x = region->center.x - theme->width / 2;
		int y = region->center.y - theme->height / 2;
		wlr_scene_node_set_position(&rect->tree->node, x, y);

		struct scaled_font_buffer *font_buffer = scaled_font_buffer_create(tree);
		node_descriptor_create(&font_buffer->scene_buffer->node,
			LAB_NODE_REGION_DROP_INDICATOR, NULL, region);
		scaled_font_buffer_update(font_buffer, region->name, theme->width,
			&rc.font_regiondrop, theme->text_color, theme->bg_color);

		x = MAX(x, region->center.x - font_buffer->width / 2);
		y = MAX(y, region->center.y - font_buffer->height / 2);
		wlr_scene_node_set_position(&font_buffer->scene_buffer->node, x, y);
	}
}

static void
destroy_region_drop_indicators(struct output *output)
{
	struct wlr_scene_node *node, *tmp;
	wl_list_for_each_safe(node, tmp,
			&output->region_drop_indicator_tree->children, link) {
		wlr_scene_node_destroy(node);
	}
}

void
regions_set_drop_indicator_enabled(bool enabled)
{
	if (!rc.regions_drop_indicator) {
		return;
	}
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		struct wlr_scene_tree *tree = output->region_drop_indicator_tree;
		if (tree) {
			if (enabled && wl_list_empty(&tree->children)
					&& !wl_list_empty(&output->regions)) {
				create_region_drop_indicators(output);
			}
			wlr_scene_node_set_enabled(&tree->node, enabled);
		}
	}
}

void
regions_reconfigure_output(struct output *output)
{
	assert(output);

	/* Evacuate views and destroy current regions */
	if (!wl_list_empty(&output->regions)) {
		regions_evacuate_output(output);
		regions_destroy(&server.seat, &output->regions);
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
regions_reconfigure(void)
{
	struct output *output;

	/* Evacuate views and initialize regions from config */
	wl_list_for_each(output, &server.outputs, link) {
		regions_reconfigure_output(output);
	}

	/* Tries to match the evacuated views to the new regions */
	desktop_arrange_all_views();
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
	destroy_region_drop_indicators(output);
}

void
regions_evacuate_output(struct output *output)
{
	assert(output);
	struct view *view;
	wl_list_for_each(view, &server.views, link) {
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
			overlay_finish(seat);
		}
		zfree(region->name);
		zfree(region);
	}
}
