// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "buffer.h"
#include "common/box.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scaled-img-buffer.h"
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
add_scene_button(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_scene_tree *parent,
		struct lab_img *imgs[LAB_BS_ALL + 1],
		int x, int y, struct view *view)
{
	struct ssd_part *button_root = add_scene_part(part_list, type);
	parent = wlr_scene_tree_create(parent);
	button_root->node = &parent->node;
	wlr_scene_node_set_position(button_root->node, x, y);

	/* Hitbox */
	float invisible[4] = { 0, 0, 0, 0 };
	add_scene_rect(part_list, type, parent,
		rc.theme->window_button_width, rc.theme->window_button_height, 0, 0,
		invisible);

	/* Icons */
	struct wlr_scene_node *nodes[LAB_BS_ALL + 1] = {0};
	for (uint8_t state_set = 0; state_set <= LAB_BS_ALL; state_set++) {
		if (!imgs[state_set]) {
			continue;
		}
		struct ssd_part *icon_part = add_scene_part(part_list, type);
		struct scaled_img_buffer *img_buffer = scaled_img_buffer_create(
			parent, imgs[state_set], rc.theme->window_button_width,
			rc.theme->window_button_height, /* padding_x */ 0);
		assert(img_buffer);
		icon_part->node = &img_buffer->scene_buffer->node;
		wlr_scene_node_set_enabled(icon_part->node, false);
		nodes[state_set] = icon_part->node;
	}
	/* Initially show non-hover, non-toggled, unrounded variant */
	wlr_scene_node_set_enabled(nodes[0], true);

	struct ssd_button *button = ssd_button_descriptor_create(button_root->node);
	button->type = type;
	button->view = view;
	button->state_set = 0;
	memcpy(button->nodes, nodes, sizeof(nodes));
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
		/* part->buffer will free itself along the scene_buffer node */
		part->buffer = NULL;
		wl_list_remove(&part->link);
		free(part);
	}
	assert(wl_list_empty(list));
}
