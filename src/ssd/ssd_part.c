// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "node.h"
#include "ssd-internal.h"

/* Internal helpers */
static void
ssd_button_destroy_notify(struct wl_listener *listener, void *data)
{
	struct ssd_button *button = wl_container_of(listener, button, destroy);
	wl_list_remove(&button->destroy.link);
	free(button);
}

/*
 * Create a new node_descriptor containing a link to a new ssd_button struct.
 * Both will be destroyed automatically once the scene_node they are attached
 * to is destroyed.
 */
static struct ssd_button *
ssd_button_descriptor_create(struct wlr_scene_node *node)
{
	/* Create new ssd_button */
	struct ssd_button *button = znew(*button);

	/* Let it destroy automatically when the scene node destroys */
	button->destroy.notify = ssd_button_destroy_notify;
	wl_signal_add(&node->events.destroy, &button->destroy);

	/* And finally attach the ssd_button to a node descriptor */
	node_descriptor_create(node, LAB_NODE_DESC_SSD_BUTTON, button);
	return button;
}

/* Internal API */
struct ssd_part *
add_scene_part(struct wl_list *part_list, enum ssd_part_type type)
{
	struct ssd_part *part = znew(*part);
	part->type = type;
	wl_list_append(part_list, &part->link);
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
		enum ssd_part_type corner_type, struct wlr_scene_tree *parent, float *bg_color,
		struct wlr_buffer *corner_buffer, struct wlr_buffer *icon_buffer,
		struct wlr_buffer *hover_buffer, int x, struct view *view)
{
	int offset_x;

	if (corner_type == LAB_SSD_PART_CORNER_TOP_LEFT) {
		offset_x = rc.theme->border_width;
	} else if (corner_type == LAB_SSD_PART_CORNER_TOP_RIGHT) {
		offset_x = 0;
	} else {
		assert(false && "invalid corner button type");
		wlr_log(WLR_ERROR, "invalid corner button type");
		abort();
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
	add_scene_button(part_list, type, parent, bg_color, icon_buffer, hover_buffer, 0, view);
	return button_root;
}

static struct wlr_box
get_scale_box(struct wlr_buffer *buffer, double container_width,
		double container_height)
{
	struct wlr_box icon_geo = {
		.width = buffer->width,
		.height = buffer->height
	};

	/* Scale down buffer if required */
	if (icon_geo.width && icon_geo.height) {
		double scale = MIN(container_width / icon_geo.width,
			container_height / icon_geo.height);
		if (scale < 1.0f) {
			icon_geo.width = (double)icon_geo.width * scale;
			icon_geo.height = (double)icon_geo.height * scale;
		}
	}

	/* Center buffer on both axis */
	icon_geo.x = (container_width - icon_geo.width) / 2;
	icon_geo.y = (container_height - icon_geo.height) / 2;

	return icon_geo;
}

struct ssd_part *
add_scene_button(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_scene_tree *parent, float *bg_color,
		struct wlr_buffer *icon_buffer, struct wlr_buffer *hover_buffer,
		int x, struct view *view)
{
	struct ssd_part *button_root = add_scene_part(part_list, type);
	parent = wlr_scene_tree_create(parent);
	button_root->node = &parent->node;
	wlr_scene_node_set_position(button_root->node, x, 0);

	/* Background */
	struct ssd_part *bg_rect = add_scene_rect(part_list, type, parent,
		SSD_BUTTON_WIDTH, rc.theme->title_height, 0, 0, bg_color);

	/* Icon */
	struct wlr_scene_tree *icon_tree = wlr_scene_tree_create(parent);
	struct wlr_box icon_geo = get_scale_box(icon_buffer,
		SSD_BUTTON_WIDTH, rc.theme->title_height);
	struct ssd_part *icon_part = add_scene_buffer(part_list, type,
		icon_tree, icon_buffer, icon_geo.x, icon_geo.y);

	/* Make sure big icons are scaled down if necessary */
	wlr_scene_buffer_set_dest_size(
		wlr_scene_buffer_from_node(icon_part->node),
		icon_geo.width, icon_geo.height);

	/* Hover icon */
	struct wlr_scene_tree *hover_tree = wlr_scene_tree_create(parent);
	wlr_scene_node_set_enabled(&hover_tree->node, false);
	struct wlr_box hover_geo = get_scale_box(hover_buffer,
		SSD_BUTTON_WIDTH, rc.theme->title_height);
	struct ssd_part *hover_part = add_scene_buffer(part_list, type,
		hover_tree, hover_buffer, hover_geo.x, hover_geo.y);

	/* Make sure big icons are scaled down if necessary */
	wlr_scene_buffer_set_dest_size(
		wlr_scene_buffer_from_node(hover_part->node),
		hover_geo.width, hover_geo.height);

	struct ssd_button *button = ssd_button_descriptor_create(button_root->node);
	button->type = type;
	button->view = view;
	button->normal = icon_part->node;
	button->hover = hover_part->node;
	button->background = bg_rect->node;
	button->toggled = NULL;
	button->toggled_hover = NULL;
	button->icon_tree = icon_tree;
	button->hover_tree = hover_tree;
	return button_root;
}

void
add_toggled_icon(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_buffer *icon_buffer, struct wlr_buffer *hover_buffer)
{
	struct ssd_part *part = ssd_get_part(part_list, type);
	struct ssd_button *button = node_ssd_button_from_node(part->node);

	/* Alternate icon */
	struct wlr_box icon_geo = get_scale_box(icon_buffer,
		SSD_BUTTON_WIDTH, rc.theme->title_height);

	struct ssd_part *alticon_part = add_scene_buffer(part_list, type,
		button->icon_tree, icon_buffer, icon_geo.x, icon_geo.y);

	wlr_scene_buffer_set_dest_size(
		wlr_scene_buffer_from_node(alticon_part->node),
		icon_geo.width, icon_geo.height);

	wlr_scene_node_set_enabled(alticon_part->node, false);

	struct wlr_box hover_geo = get_scale_box(hover_buffer,
		SSD_BUTTON_WIDTH, rc.theme->title_height);
	struct ssd_part *althover_part = add_scene_buffer(part_list, type,
		button->hover_tree, hover_buffer, hover_geo.x, hover_geo.y);

	wlr_scene_buffer_set_dest_size(
		wlr_scene_buffer_from_node(althover_part->node),
		hover_geo.width, hover_geo.height);

	wlr_scene_node_set_enabled(althover_part->node, false);

	button->toggled = alticon_part->node;
	button->toggled_hover = althover_part->node;
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
		/* part->buffer will free itself along the scene_buffer node */
		part->buffer = NULL;
		if (part->geometry) {
			free(part->geometry);
			part->geometry = NULL;
		}
		wl_list_remove(&part->link);
		free(part);
	}
	assert(wl_list_empty(list));
}
