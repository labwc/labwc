// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "labwc.h"
#include "ssd.h"

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
	struct wlr_scene_node *parent, int width, int height,
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
	struct wlr_scene_node *parent, struct wlr_buffer *buffer,
	int x, int y)
{
	struct ssd_part *part = add_scene_part(list, type);
	part->node = &wlr_scene_buffer_create(parent, buffer)->node;
	wlr_scene_node_set_position(part->node, x, y);
	return part;
}

static void
finish_scene_button(struct wl_list *part_list, enum ssd_part_type type,
	struct wlr_scene_node *parent, struct wlr_buffer *icon_buffer)
{
	float hover_bg[4] = {0.15f, 0.15f, 0.15f, 0.3f};

	/* Icon */
	add_scene_buffer(part_list, type, parent, icon_buffer,
		(BUTTON_WIDTH - icon_buffer->width) / 2,
		(rc.theme->title_height - icon_buffer->height) / 2);

	/* Hover overlay */
	struct ssd_part *hover_part;
	hover_part = add_scene_rect(part_list, type, parent, BUTTON_WIDTH,
		rc.theme->title_height, 0, 0, hover_bg);
	wlr_scene_node_set_enabled(hover_part->node, false);
}

struct ssd_part *
add_scene_button_corner(struct wl_list *part_list, enum ssd_part_type type,
	struct wlr_scene_node *parent, struct wlr_buffer *corner_buffer,
	struct wlr_buffer *icon_buffer, int x)
{
	struct ssd_part *button_root = add_scene_part(part_list, type);
	parent = &wlr_scene_tree_create(parent)->node;
	button_root->node = parent;
	wlr_scene_node_set_position(button_root->node, x, 0);

	int offset_x;
	if (type == LAB_SSD_BUTTON_WINDOW_MENU) {
		offset_x = rc.theme->border_width;
	} else if (type == LAB_SSD_BUTTON_CLOSE) {
		offset_x = 0;
	} else {
		assert(false && "invalid corner button type");
	}

	struct ssd_part *part;
	/*
	 * Background, y adjusted for border_width which is
	 * already included in rendered theme.c / corner_buffer
	 */
	part = add_scene_buffer(part_list, type, parent, corner_buffer,
		-offset_x, -rc.theme->border_width);
	finish_scene_button(part_list, type, parent, icon_buffer);
	return part;
}

struct ssd_part *
add_scene_button(struct wl_list *part_list, enum ssd_part_type type,
	struct wlr_scene_node *parent, float *bg_color,
	struct wlr_buffer *icon_buffer, int x)
{
	struct ssd_part *button_root = add_scene_part(part_list, type);
	parent = &wlr_scene_tree_create(parent)->node;
	button_root->node = parent;
	wlr_scene_node_set_position(button_root->node, x, 0);

	struct ssd_part *part;
	/* Background */
	part = add_scene_rect(part_list, type, parent,
		BUTTON_WIDTH, rc.theme->title_height, 0, 0, bg_color);
	finish_scene_button(part_list, type, parent, icon_buffer);
	return part;
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
