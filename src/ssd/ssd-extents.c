// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <pixman.h>
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "labwc.h"
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
	int extended_area = SSD_EXTENDED_AREA;

	ssd->extents.tree = wlr_scene_tree_create(ssd->tree);
	struct wlr_scene_tree *parent = ssd->extents.tree;
	if (view->fullscreen || view->maximized == VIEW_AXIS_BOTH) {
		wlr_scene_node_set_enabled(&parent->node, false);
	}
	wl_list_init(&ssd->extents.parts);
	wlr_scene_node_set_position(&parent->node,
		-(theme->border_width + extended_area),
		-(ssd->titlebar.height + theme->border_width + extended_area));

	add_extent(part_list, LAB_SSD_PART_TOP, parent);
	add_extent(part_list, LAB_SSD_PART_LEFT, parent);
	add_extent(part_list, LAB_SSD_PART_RIGHT, parent);
	add_extent(part_list, LAB_SSD_PART_BOTTOM, parent);

	/* Initial manual update to keep X11 applications happy */
	ssd_extents_update(ssd);
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
	int extended_area = SSD_EXTENDED_AREA;

	struct wlr_box part_box;
	struct wlr_box result_box;
	struct ssd_part *part;
	struct wlr_scene_rect *rect;
	struct wlr_box target;

	/* Make sure we update the y offset based on titlebar shown / hidden */
	wlr_scene_node_set_position(&ssd->extents.tree->node,
		-(theme->border_width + extended_area),
		-(ssd->titlebar.height + theme->border_width + extended_area));

	/*
	 * Convert all output usable areas that the
	 * view is currently on into a pixman region
	 */
	int nrects;
	pixman_region32_t usable;
	pixman_region32_t intersection;
	pixman_region32_init(&usable);
	pixman_region32_init(&intersection);
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

	/* Remember base layout coordinates */
	int base_x, base_y;
	wlr_scene_node_coords(&ssd->extents.tree->node, &base_x, &base_y);

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

		/* Get layout geometry of what the part *should* be */
		part_box.x = base_x + target.x;
		part_box.y = base_y + target.y;
		part_box.width = target.width;
		part_box.height = target.height;

		/* Constrain part to output->usable_area */
		pixman_region32_clear(&intersection);
		pixman_region32_intersect_rect(&intersection, &usable,
			part_box.x, part_box.y, part_box.width, part_box.height);
		const pixman_box32_t *inter_rects =
			pixman_region32_rectangles(&intersection, &nrects);

		if (nrects == 0) {
			/* Not visible */
			wlr_scene_node_set_enabled(part->node, false);
			continue;
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
		result_box = (struct wlr_box) {
			.x = inter_rects[0].x1,
			.y = inter_rects[0].y1,
			.width = inter_rects[0].x2 - inter_rects[0].x1,
			.height = inter_rects[0].y2 - inter_rects[0].y1
		};

		if (!part->node->enabled) {
			wlr_scene_node_set_enabled(part->node, true);
		}

		if (part_box.width != result_box.width
				|| part_box.height != result_box.height) {
			/* Partly visible */
			wlr_scene_rect_set_size(rect, result_box.width,
				result_box.height);
			wlr_scene_node_set_position(part->node,
				target.x + (result_box.x - part_box.x),
				target.y + (result_box.y - part_box.y));
		} else {
			/* Fully visible */
			wlr_scene_node_set_position(part->node, target.x, target.y);
			wlr_scene_rect_set_size(rect, target.width, target.height);
		}
	}
	pixman_region32_fini(&intersection);
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
