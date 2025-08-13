// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <pixman.h>
#include <wlr/types/wlr_scene.h>
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

static struct ssd_part *
add_extent(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_scene_tree *parent)
{
	float invisible[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	struct ssd_part *part = add_scene_part(part_list, type);
	part->node = &wlr_scene_rect_create(parent, 0, 0, invisible)->node;
	return part;
}

void
ssd_extents_create(struct ssd *ssd)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	struct wl_list *part_list = &ssd->extents.parts;

	int border_width = MAX(0, MAX(rc.resize_minimum_area, theme->border_width));

	ssd->extents.tree = wlr_scene_tree_create(ssd->tree);
	struct wlr_scene_tree *parent = ssd->extents.tree;
	if (view->fullscreen || view->maximized == VIEW_AXIS_BOTH) {
		wlr_scene_node_set_enabled(&parent->node, false);
	}
	wl_list_init(&ssd->extents.parts);
	wlr_scene_node_set_position(&parent->node,
		-border_width, -(ssd->titlebar.height + border_width));

	add_extent(part_list, LAB_SSD_PART_TOP, parent);
	add_extent(part_list, LAB_SSD_PART_LEFT, parent);
	add_extent(part_list, LAB_SSD_PART_RIGHT, parent);
	add_extent(part_list, LAB_SSD_PART_BOTTOM, parent);

	/* Initial manual update to keep X11 applications happy */
	ssd_extents_update(ssd);
}

static void
resize_extent_within_usable(struct wlr_scene_rect *rect,
		pixman_region32_t *usable, int x, int y, int w, int h)
{
	pixman_region32_t intersection;
	pixman_region32_init(&intersection);
	/* Constrain part to output->usable_area */
	pixman_region32_intersect_rect(&intersection, usable, x, y, w, h);
	int nrects;
	const pixman_box32_t *inter_rects =
		pixman_region32_rectangles(&intersection, &nrects);

	if (nrects == 0) {
		/* Not visible */
		wlr_scene_node_set_enabled(&rect->node, false);
		goto out;
	}

	/*
	 * For each edge, the invisible grab area is resized
	 * to not cover layer-shell clients such as panels.
	 * However, only one resize operation is used per edge,
	 * so if a window is in the unlikely position that it
	 * is near a panel but also overspills onto another screen,
	 * the invisible grab-area on the other screen would be
	 * smaller than would normally be the case.
	 *
	 * Thus only use the first intersecting rect, this is
	 * a compromise as it doesn't require us to create
	 * multiple scene rects for a given extent edge
	 * and still works in 95% of the cases.
	 */
	struct wlr_box result_box = (struct wlr_box) {
		.x = inter_rects[0].x1,
		.y = inter_rects[0].y1,
		.width = inter_rects[0].x2 - inter_rects[0].x1,
		.height = inter_rects[0].y2 - inter_rects[0].y1
	};

	wlr_scene_node_set_enabled(&rect->node, true);

	wlr_scene_node_set_position(&rect->node, result_box.x, result_box.y);
	wlr_scene_rect_set_size(rect, result_box.width, result_box.height);

out:
	pixman_region32_fini(&intersection);
}

void
ssd_extents_update(struct ssd *ssd)
{
	struct view *view = ssd->view;
	if (view->fullscreen || view->maximized == VIEW_AXIS_BOTH) {
		wlr_scene_node_set_enabled(&ssd->extents.tree->node, false);
		return;
	}
	if (!ssd->extents.tree->node.enabled) {
		wlr_scene_node_set_enabled(&ssd->extents.tree->node, true);
	}

	if (!view->output) {
		return;
	}

	struct theme *theme = view->server->theme;

	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
	int full_height = height + theme->border_width * 2 + ssd->titlebar.height;
	int full_width = width + 2 * theme->border_width;
	int border_width = MAX(rc.resize_minimum_area, theme->border_width);
	int extended_area = MAX(0, rc.resize_minimum_area - theme->border_width);

	struct ssd_part *part;
	struct wlr_scene_rect *rect;
	struct wlr_box target;

	/* Make sure we update the y offset based on titlebar shown / hidden */
	wlr_scene_node_set_position(&ssd->extents.tree->node,
		-border_width, -(ssd->titlebar.height + border_width));

	/*
	 * Convert all output usable areas that the
	 * view is currently on into a pixman region
	 */
	pixman_region32_t usable;
	pixman_region32_init(&usable);
	struct output *output;
	wl_list_for_each(output, &view->server->outputs, link) {
		if (!view_on_output(view, output)) {
			continue;
		}
		struct wlr_box usable_area =
			output_usable_area_in_layout_coords(output);
		pixman_region32_union_rect(&usable, &usable, usable_area.x,
			usable_area.y, usable_area.width, usable_area.height);
	}

	/* Move usable area to the coordinate system of extents */
	int base_x, base_y;
	wlr_scene_node_coords(&ssd->extents.tree->node, &base_x, &base_y);
	pixman_region32_translate(&usable, -base_x, -base_y);

	wl_list_for_each(part, &ssd->extents.parts, link) {
		rect = wlr_scene_rect_from_node(part->node);
		switch (part->type) {
		case LAB_SSD_PART_TOP:
			target.x = 0;
			target.y = 0;
			target.width = full_width + extended_area * 2;
			target.height = extended_area;
			break;
		case LAB_SSD_PART_LEFT:
			target.x = 0;
			target.y = extended_area;
			target.width = extended_area;
			target.height = full_height;
			break;
		case LAB_SSD_PART_RIGHT:
			target.x = extended_area + full_width;
			target.y = extended_area;
			target.width = extended_area;
			target.height = full_height;
			break;
		case LAB_SSD_PART_BOTTOM:
			target.x = 0;
			target.y = extended_area + full_height;
			target.width = full_width + extended_area * 2;
			target.height = extended_area;
			break;
		default:
			/* not reached */
			assert(false);
			/* suppress warnings with NDEBUG */
			target = (struct wlr_box){0};
		}

		resize_extent_within_usable(rect, &usable,
			target.x, target.y, target.width, target.height);
	}
	pixman_region32_fini(&usable);
}

void
ssd_extents_destroy(struct ssd *ssd)
{
	if (!ssd->extents.tree) {
		return;
	}

	ssd_destroy_parts(&ssd->extents.parts);
	wlr_scene_node_destroy(&ssd->extents.tree->node);
	ssd->extents.tree = NULL;
}
