// SPDX-License-Identifier: GPL-2.0-only

#include "labwc.h"
#include "ssd.h"
#include "theme.h"
#include "common/scene-helpers.h"

void
ssd_extents_create(struct view *view)
{
	struct theme *theme = view->server->theme;
	float invisible[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	struct wl_list *part_list = &view->ssd.extents.parts;
	int width = view->w;
	int height = view->h;
	int full_height = height + theme->border_width * 2 + theme->title_height;
	int full_width = width + theme->border_width * 2;
	int extended_area = EXTENDED_AREA;
	int corner_size = extended_area + theme->border_width + BUTTON_WIDTH / 2;
	int side_width = full_width + extended_area * 2 - corner_size * 2;
	int side_height = full_height + extended_area * 2 - corner_size * 2;

	view->ssd.extents.tree = wlr_scene_tree_create(&view->ssd.tree->node);
	struct wlr_scene_node *parent = &view->ssd.extents.tree->node;
	if (view->maximized || view->fullscreen) {
		wlr_scene_node_set_enabled(parent, false);
	}
	wl_list_init(&view->ssd.extents.parts);
	wlr_scene_node_set_position(parent, -(theme->border_width + extended_area),
		-(theme->title_height + theme->border_width + extended_area));

	/* Top */
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_TOP_LEFT, parent,
		corner_size, corner_size,
		0, 0, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_TOP, parent,
		side_width, extended_area,
		corner_size, 0, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_TOP_RIGHT, parent,
		corner_size, corner_size,
		corner_size + side_width, 0, invisible);

	/* Sides */
	add_scene_rect(part_list, LAB_SSD_PART_LEFT, parent,
		extended_area, side_height,
		0, corner_size, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_RIGHT, parent,
		extended_area, side_height,
		corner_size + side_width, corner_size, invisible);

	/* Bottom */
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_BOTTOM_LEFT, parent,
		corner_size, corner_size,
		0, corner_size + side_height, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_BOTTOM, parent,
		side_width, extended_area,
		corner_size, extended_area + full_height, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_BOTTOM_RIGHT, parent,
		corner_size, corner_size,
		corner_size + side_width, corner_size + side_height, invisible);
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

	struct theme *theme = view->server->theme;

	int width = view->w;
	int height = view->h;
	int full_height = height + theme->border_width * 2 + theme->title_height;
	int full_width = width + 2 * theme->border_width;
	int extended_area = EXTENDED_AREA;
	int corner_size = extended_area + theme->border_width + BUTTON_WIDTH / 2;
	int side_width = full_width + extended_area * 2 - corner_size * 2;
	int side_height = full_height + extended_area * 2 - corner_size * 2;

	struct ssd_part *part;
	struct wlr_scene_rect *rect;
	wl_list_for_each(part, &view->ssd.extents.parts, link) {
		rect = lab_wlr_scene_get_rect(part->node);
		switch (part->type) {
		case LAB_SSD_PART_TOP:
			wlr_scene_rect_set_size(rect, side_width, extended_area);
			continue;
		case LAB_SSD_PART_CORNER_TOP_RIGHT:
			wlr_scene_node_set_position(
				part->node, corner_size + side_width, 0);
			continue;
		case LAB_SSD_PART_LEFT:
			wlr_scene_rect_set_size(rect, extended_area, side_height);
			continue;
		case LAB_SSD_PART_RIGHT:
			wlr_scene_rect_set_size(rect, extended_area, side_height);
			wlr_scene_node_set_position(
				part->node, extended_area + full_width, corner_size);
			continue;
		case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
			wlr_scene_node_set_position(
				part->node, 0, corner_size + side_height);
			continue;
		case LAB_SSD_PART_BOTTOM:
			wlr_scene_rect_set_size(rect, side_width, extended_area);
			wlr_scene_node_set_position(
				part->node, corner_size, extended_area + full_height);
			continue;
		case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
			wlr_scene_node_set_position(part->node,
				corner_size + side_width, corner_size + side_height);
			continue;
		default:
			continue;
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
