// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "config/rcxml.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scaled-icon-buffer.h"
#include "common/scaled-img-buffer.h"
#include "node.h"
#include "ssd-internal.h"

/* Internal helpers */
static void
handle_node_destroy(struct wl_listener *listener, void *data)
{
	struct ssd_part *part = wl_container_of(listener, part, node_destroy);
	wl_list_remove(&part->node_destroy.link);

	struct ssd_part_button *button = button_try_from_ssd_part(part);
	if (button) {
		wl_list_remove(&button->link);
	}

	free(part);
}

/* Internal API */

/*
 * Create a new node_descriptor containing a link to a new ssd_part struct.
 * Both will be destroyed automatically once the scene_node they are attached
 * to is destroyed.
 */
static void
init_ssd_part(struct ssd_part *part, enum ssd_part_type type,
		struct view *view, struct wlr_scene_node *node)
{
	part->type = type;
	part->node = node;
	part->view = view;

	node_descriptor_create(node, LAB_NODE_DESC_SSD_PART, part);
	part->node_destroy.notify = handle_node_destroy;
	wl_signal_add(&node->events.destroy, &part->node_destroy);
}

struct ssd_part *
attach_ssd_part(enum ssd_part_type type, struct view *view,
		struct wlr_scene_node *node)
{
	assert(!ssd_part_contains(LAB_SSD_BUTTON, type));
	struct ssd_part *part = znew(*part);
	init_ssd_part(part, type, view, node);
	return part;
}

struct ssd_part_button *
attach_ssd_part_button(struct wl_list *button_parts, enum ssd_part_type type,
		struct wlr_scene_tree *parent,
		struct lab_img *imgs[LAB_BS_ALL + 1],
		int x, int y, struct view *view)
{
	struct wlr_scene_tree *root = wlr_scene_tree_create(parent);
	wlr_scene_node_set_position(&root->node, x, y);

	assert(ssd_part_contains(LAB_SSD_BUTTON, type));
	struct ssd_part_button *button = znew(*button);
	init_ssd_part(&button->base, type, view, &root->node);
	wl_list_append(button_parts, &button->link);

	/* Hitbox */
	float invisible[4] = { 0, 0, 0, 0 };
	wlr_scene_rect_create(root, rc.theme->window_button_width,
		rc.theme->window_button_height, invisible);

	/* Icons */
	int button_width = rc.theme->window_button_width;
	int button_height = rc.theme->window_button_height;
	/*
	 * Ensure a small amount of horizontal padding within the button
	 * area (2px on each side with the default 26px button width).
	 * A new theme setting could be added to configure this. Using
	 * an existing setting (padding.width or window.button.spacing)
	 * was considered, but these settings have distinct purposes
	 * already and are zero by default.
	 */
	int icon_padding = button_width / 10;

	if (type == LAB_SSD_BUTTON_WINDOW_ICON) {
		struct scaled_icon_buffer *icon_buffer =
			scaled_icon_buffer_create(root, view->server,
				button_width - 2 * icon_padding, button_height);
		assert(icon_buffer);
		struct wlr_scene_node *icon_node = &icon_buffer->scene_buffer->node;
		scaled_icon_buffer_set_view(icon_buffer, view);
		wlr_scene_node_set_position(icon_node, icon_padding, 0);
		button->window_icon = icon_buffer;
	} else {
		for (uint8_t state_set = LAB_BS_DEFAULT;
				state_set <= LAB_BS_ALL; state_set++) {
			if (!imgs[state_set]) {
				continue;
			}
			struct scaled_img_buffer *img_buffer = scaled_img_buffer_create(
				root, imgs[state_set], rc.theme->window_button_width,
				rc.theme->window_button_height);
			assert(img_buffer);
			struct wlr_scene_node *icon_node = &img_buffer->scene_buffer->node;
			wlr_scene_node_set_enabled(icon_node, false);
			button->img_buffers[state_set] = img_buffer;
		}
		/* Initially show non-hover, non-toggled, unrounded variant */
		wlr_scene_node_set_enabled(
			&button->img_buffers[LAB_BS_DEFAULT]->scene_buffer->node, true);
	}

	return button;
}

struct ssd_part_button *
button_try_from_ssd_part(struct ssd_part *part)
{
	if (ssd_part_contains(LAB_SSD_BUTTON, part->type)) {
		return (struct ssd_part_button *)part;
	}
	return NULL;
}
