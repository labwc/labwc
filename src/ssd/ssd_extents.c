// SPDX-License-Identifier: GPL-2.0-only

#include "labwc.h"
#include "ssd.h"
#include "theme.h"
#include "common/scene-helpers.h"

static struct ssd_part *
add_extent(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_scene_node *parent)
{
	float invisible[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	struct ssd_part *part = add_scene_part(part_list, type);
	/*
	 * Extents need additional geometry to enable dynamic
	 * resize based on position and output->usable_area.
	 *
	 * part->geometry will get free'd automatically in ssd_destroy_parts().
	 */
	part->node = &wlr_scene_rect_create(parent, 0, 0, invisible)->node;
	part->geometry = calloc(1, sizeof(struct wlr_box));
	return part;
}

static void
lab_wlr_output_layout_layout_coords(struct wlr_output_layout *layout,
		struct wlr_output *output, int *x, int *y)
{
	struct wlr_output_layout_output *l_output;
	l_output = wlr_output_layout_get(layout, output);
	*x += l_output->x;
	*y += l_output->y;
}

void
ssd_extents_create(struct view *view)
{
	struct theme *theme = view->server->theme;
	struct wl_list *part_list = &view->ssd.extents.parts;
	int extended_area = EXTENDED_AREA;
	int corner_size = extended_area + theme->border_width + BUTTON_WIDTH / 2;

	view->ssd.extents.tree = wlr_scene_tree_create(&view->ssd.tree->node);
	struct wlr_scene_node *parent = &view->ssd.extents.tree->node;
	if (view->maximized || view->fullscreen) {
		wlr_scene_node_set_enabled(parent, false);
	}
	wl_list_init(&view->ssd.extents.parts);
	wlr_scene_node_set_position(parent, -(theme->border_width + extended_area),
		-(theme->title_height + theme->border_width + extended_area));

	/* Initialize parts and set constant values for targeted geometry */
	struct ssd_part *p;

	/* Top */
	p = add_extent(part_list, LAB_SSD_PART_CORNER_TOP_LEFT, parent);
	p->geometry->width = corner_size;
	p->geometry->height = corner_size;

	p = add_extent(part_list, LAB_SSD_PART_TOP, parent);
	p->geometry->x = corner_size;
	p->geometry->height = extended_area;

	p = add_extent(part_list, LAB_SSD_PART_CORNER_TOP_RIGHT, parent);
	p->geometry->width = corner_size;
	p->geometry->height = corner_size;

	/* Sides */
	p = add_extent(part_list, LAB_SSD_PART_LEFT, parent);
	p->geometry->y = corner_size;
	p->geometry->width = extended_area;

	p = add_extent(part_list, LAB_SSD_PART_RIGHT, parent);
	p->geometry->y = corner_size;
	p->geometry->width = extended_area;

	/* Bottom */
	p = add_extent(part_list, LAB_SSD_PART_CORNER_BOTTOM_LEFT, parent);
	p->geometry->width = corner_size;
	p->geometry->height = corner_size;

	p = add_extent(part_list, LAB_SSD_PART_BOTTOM, parent);
	p->geometry->x = corner_size;
	p->geometry->height = extended_area;

	p = add_extent(part_list, LAB_SSD_PART_CORNER_BOTTOM_RIGHT, parent);
	p->geometry->width = corner_size;
	p->geometry->height = corner_size;

	/* Initial manual update to keep X11 applications happy */
	ssd_extents_update(view);
}

void
ssd_extents_update(struct view *view)
{
	if (view->maximized || view->fullscreen) {
		wlr_scene_node_set_enabled(&view->ssd.extents.tree->node, false);
		return;
	}
	if (!view->ssd.extents.tree->node.state.enabled) {
		wlr_scene_node_set_enabled(&view->ssd.extents.tree->node, true);
	}

	if (!view->output) {
		return;
	}

	struct theme *theme = view->server->theme;

	int width = view->w;
	int height = view->h;
	int full_height = height + theme->border_width * 2 + theme->title_height;
	int full_width = width + 2 * theme->border_width;
	int extended_area = EXTENDED_AREA;
	int corner_size = extended_area + theme->border_width + BUTTON_WIDTH / 2;
	int side_width = full_width + extended_area * 2 - corner_size * 2;
	int side_height = full_height + extended_area * 2 - corner_size * 2;

	struct wlr_box part_box;
	struct wlr_box result_box;
	struct ssd_part *part;
	struct wlr_scene_rect *rect;

	/* Convert usable area into layout coordinates */
	struct wlr_box usable_area;
	memcpy(&usable_area, &view->output->usable_area, sizeof(struct wlr_box));
	lab_wlr_output_layout_layout_coords(view->server->output_layout,
		view->output->wlr_output, &usable_area.x, &usable_area.y);

	/* Remember base layout coordinates */
	int base_x, base_y;
	wlr_scene_node_coords(&view->ssd.extents.tree->node, &base_x, &base_y);

	struct wlr_box *target;
	wl_list_for_each(part, &view->ssd.extents.parts, link) {
		rect = lab_wlr_scene_get_rect(part->node);
		target = part->geometry;
		switch (part->type) {
		case LAB_SSD_PART_TOP:
			target->width = side_width;
			break;
		case LAB_SSD_PART_CORNER_TOP_RIGHT:
			target->x = corner_size + side_width;
			break;
		case LAB_SSD_PART_LEFT:
			target->height = side_height;
			break;
		case LAB_SSD_PART_RIGHT:
			target->x = extended_area + full_width;
			target->height = side_height;
			break;
		case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
			target->y = corner_size + side_height;
			break;
		case LAB_SSD_PART_BOTTOM:
			target->width = side_width;
			target->y = extended_area + full_height;
			break;
		case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
			target->x = corner_size + side_width;
			target->y = corner_size + side_height;
			break;
		default:
			break;
		}

		/* Get layout geometry of what the part *should* be */
		part_box.x = base_x + target->x;
		part_box.y = base_y + target->y;
		part_box.width = target->width;
		part_box.height = target->height;

		/* Constrain part to output->usable_area */
		if (!wlr_box_intersection(&result_box, &part_box, &usable_area)) {
			/* Not visible */
			wlr_scene_node_set_enabled(part->node, false);
			continue;
		} else if (!part->node->state.enabled) {
			wlr_scene_node_set_enabled(part->node, true);
		}

		if (part_box.width != result_box.width
				|| part_box.height != result_box.height) {
			/* Partly visible */
			wlr_scene_rect_set_size(rect, result_box.width, result_box.height);
			wlr_scene_node_set_position(part->node,
				target->x + (result_box.x - part_box.x),
				target->y + (result_box.y - part_box.y));
			continue;
		}

		/* Fully visible */
		if (target->x != part->node->state.x
				|| target->y != part->node->state.y) {
			wlr_scene_node_set_position(part->node, target->x, target->y);
		}
		if (target->width != rect->width || target->height != rect->height) {
			wlr_scene_rect_set_size(rect, target->width, target->height);
		}
	}
}

void
ssd_extents_destroy(struct view *view)
{
	if (!view->ssd.extents.tree) {
		return;
	}

	ssd_destroy_parts(&view->ssd.extents.parts);
	wlr_scene_node_destroy(&view->ssd.extents.tree->node);
	view->ssd.extents.tree = NULL;
}
