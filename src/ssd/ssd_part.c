// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "labwc.h"
#include "ssd.h"
#include "node.h"

struct ssd_part *
add_scene_part(struct wl_list *part_list, enum ssd_part_type type)
{
	struct ssd_part *part = calloc(1, sizeof(struct ssd_part));
	part->type = type;
	wl_list_insert(part_list->prev, &part->link);
	return part;
}

struct ssd_part *
add_scene_rect(struct wl_list *list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, int width, int height,
	int x, int y, float color[4])
{
	/*
	 * When initialized without surface being mapped,
	 * size may be negative. Just set to 0, next call
	 * to ssd_*_update() will update the rect to use
	 * its correct size.
	 */
	width = width >= 0 ? width : 0;
	height = height >= 0 ? height : 0;

	struct ssd_part *part = add_scene_part(list, type);
	part->node = &wlr_scene_rect_create(
		parent, width, height, color)->node;
	wlr_scene_node_set_position(part->node, x, y);
	return part;
}

struct ssd_part *
add_scene_buffer(struct wl_list *list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, struct wlr_buffer *buffer,
	int x, int y)
{
	struct ssd_part *part = add_scene_part(list, type);
	part->node = &wlr_scene_buffer_create(parent, buffer)->node;
	wlr_scene_node_set_position(part->node, x, y);
	return part;
}

struct ssd_part *
add_scene_button_corner(struct wl_list *part_list, enum ssd_part_type type,
		enum ssd_part_type corner_type, struct wlr_scene_tree *parent,
		struct wlr_buffer *corner_buffer, struct wlr_buffer *icon_buffer,
		int x, struct view *view)
{
	int offset_x;
	float invisible[4] = { 0, 0, 0, 0 };

	if (corner_type == LAB_SSD_PART_CORNER_TOP_LEFT) {
		offset_x = rc.theme->border_width;
	} else if (corner_type == LAB_SSD_PART_CORNER_TOP_RIGHT) {
		offset_x = 0;
	} else {
		assert(false && "invalid corner button type");
	}

	struct ssd_part *button_root = add_scene_part(part_list, corner_type);
	parent = wlr_scene_tree_create(parent);
	button_root->node = &parent->node;
	wlr_scene_node_set_position(button_root->node, x, 0);

	/*
	 * Background, x and y adjusted for border_width which is
	 * already included in rendered theme.c / corner_buffer
	 */
	add_scene_buffer(part_list, corner_type, parent, corner_buffer,
		-offset_x, -rc.theme->border_width);

	/* Finally just put a usual theme button on top, using an invisible hitbox */
	add_scene_button(part_list, type, parent, invisible, icon_buffer, 0, view);
	return button_root;
}

struct ssd_part *
add_scene_button(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_scene_tree *parent, float *bg_color,
		struct wlr_buffer *icon_buffer, int x, struct view *view)
{
	struct ssd_part *part;
	float hover_bg[4] = {0.15f, 0.15f, 0.15f, 0.3f};

	struct ssd_part *button_root = add_scene_part(part_list, type);
	parent = wlr_scene_tree_create(parent);
	button_root->node = &parent->node;
	wlr_scene_node_set_position(button_root->node, x, 0);
	node_descriptor_create(button_root->node, LAB_NODE_DESC_SSD_BUTTON, view);

	/* Background */
	part = add_scene_rect(part_list, type, parent,
		BUTTON_WIDTH, rc.theme->title_height, 0, 0, bg_color);

	/* Icon */
	add_scene_buffer(part_list, type, parent, icon_buffer,
		(BUTTON_WIDTH - icon_buffer->width) / 2,
		(rc.theme->title_height - icon_buffer->height) / 2);

	/* Hover overlay */
	part = add_scene_rect(part_list, type, parent, BUTTON_WIDTH,
		rc.theme->title_height, 0, 0, hover_bg);
	wlr_scene_node_set_enabled(part->node, false);

	return button_root;
}

struct ssd_part *
ssd_get_part(struct wl_list *part_list, enum ssd_part_type type)
{
	struct ssd_part *part;
	wl_list_for_each(part, part_list, link) {
		if (part->type == type) {
			return part;
		}
	}
	return NULL;
}

void
ssd_destroy_parts(struct wl_list *list)
{
	struct ssd_part *part, *tmp;
	wl_list_for_each_reverse_safe(part, tmp, list, link) {
		if (part->node) {
			wlr_scene_node_destroy(part->node);
			part->node = NULL;
		}
		if (part->buffer) {
			wlr_buffer_drop(&part->buffer->base);
			part->buffer = NULL;
		}
		if (part->geometry) {
			free(part->geometry);
			part->geometry = NULL;
		}
		wl_list_remove(&part->link);
		free(part);
	}
	assert(wl_list_empty(list));
}
