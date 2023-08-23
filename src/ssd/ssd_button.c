// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h" /* for MIN() */
#include "node.h"
#include "ssd-internal.h"

struct ssd_rounded {
	struct ssd_button *button;
	struct wlr_scene_node *node;

	struct wl_listener destroy;
};

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

static void
ssd_rounded_destroy_notify(struct wl_listener *listener, void *data)
{
	struct ssd_rounded *rounded = wl_container_of(listener, rounded, destroy);
	wl_list_remove(&rounded->destroy.link);
	free(rounded);
}

static struct ssd_rounded *
ssd_rounded_descriptor_create(struct wlr_scene_node *node)
{
	struct ssd_rounded *rounded = znew(*rounded);
	rounded->destroy.notify = ssd_rounded_destroy_notify;
	wl_signal_add(&node->events.destroy, &rounded->destroy);
	node_descriptor_create(node, LAB_NODE_DESC_SSD_ROUNDED, rounded);
	return rounded;
}

static struct wlr_box
get_scale_box(struct wlr_buffer *buffer, double container_width, double container_height)
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
ssd_button_add_corner(struct wl_list *part_list, enum ssd_part_type type,
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

	struct ssd_part *button_corner_root = add_scene_part(part_list, corner_type);
	parent = wlr_scene_tree_create(parent);
	button_corner_root->node = &parent->node;
	wlr_scene_node_set_position(button_corner_root->node, x, 0);

	/*
	 * Background, x and y adjusted for border_width which is
	 * already included in rendered theme.c / corner_buffer
	 */
	struct ssd_part *button_rounded = add_scene_buffer(part_list, corner_type,
		parent, corner_buffer, -offset_x, -rc.theme->border_width);

	/* Finally just put a usual theme button on top, using an invisible hitbox */
	struct ssd_part *button_part =
		ssd_button_add(part_list, type, parent, invisible, icon_buffer, 0, view);

	/* And store the rounded corner scene node */
	struct ssd_button *ssd_button = node_ssd_button_from_node(button_part->node);
	struct ssd_rounded *rounded = ssd_rounded_descriptor_create(button_corner_root->node);
	rounded->node = button_rounded->node;
	rounded->button = ssd_button;

	return button_corner_root;
}

struct ssd_part *
ssd_button_add(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_scene_tree *parent, float *bg_color,
		struct wlr_buffer *icon_buffer, int x, struct view *view)
{
	struct wlr_scene_node *hover;
	float hover_bg[4] = {0.15f, 0.15f, 0.15f, 0.3f};

	struct ssd_part *button_root = add_scene_part(part_list, type);
	parent = wlr_scene_tree_create(parent);
	button_root->node = &parent->node;
	wlr_scene_node_set_position(button_root->node, x, 0);

	/* Background */
	struct ssd_part *bg_rect = add_scene_rect(part_list, type, parent,
		SSD_BUTTON_WIDTH, rc.theme->title_height, 0, 0, bg_color);

	/* Icon */
	struct wlr_box icon_geo = get_scale_box(icon_buffer,
		SSD_BUTTON_WIDTH, rc.theme->title_height);
	struct ssd_part *icon_part = add_scene_buffer(part_list, type,
		parent, icon_buffer, icon_geo.x, icon_geo.y);

	/* Make sure big icons are scaled down if necessary */
	wlr_scene_buffer_set_dest_size(
		wlr_scene_buffer_from_node(icon_part->node),
		icon_geo.width, icon_geo.height);

	/* Hover overlay */
	hover = add_scene_rect(part_list, type, parent, SSD_BUTTON_WIDTH,
		rc.theme->title_height, 0, 0, hover_bg)->node;
	wlr_scene_node_set_enabled(hover, false);

	struct ssd_button *button = ssd_button_descriptor_create(button_root->node);
	button->type = type;
	button->view = view;
	button->hover = hover;
	button->background = bg_rect->node;
	return button_root;
}

void
ssd_button_enable_rounded_corner(struct ssd_part *corner_tree, float *bg_color, bool enable)
{
	assert(corner_tree);

	struct ssd_rounded *rounded = node_ssd_rounded_from_node(corner_tree->node);

	/* Toggle background between invisible and titlebar background color */
	struct wlr_scene_rect *rect = lab_wlr_scene_get_rect(rounded->button->background);
	wlr_scene_rect_set_color(rect, enable ? (float[4]) {0, 0, 0, 0} : bg_color);

	/* Toggle rounded corner image itself */
	wlr_scene_node_set_enabled(rounded->node, enable);
}
