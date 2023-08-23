// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include "common/mem.h"
#include "node.h"

static void
descriptor_destroy(struct node_descriptor *node_descriptor)
{
	if (!node_descriptor) {
		return;
	}
	wl_list_remove(&node_descriptor->destroy.link);
	free(node_descriptor);
}

static void
destroy_notify(struct wl_listener *listener, void *data)
{
	struct node_descriptor *node_descriptor =
		wl_container_of(listener, node_descriptor, destroy);
	descriptor_destroy(node_descriptor);
}

void
node_descriptor_create(struct wlr_scene_node *scene_node,
		enum node_descriptor_type type, void *data)
{
	struct node_descriptor *node_descriptor = znew(*node_descriptor);
	node_descriptor->type = type;
	node_descriptor->data = data;
	node_descriptor->destroy.notify = destroy_notify;
	wl_signal_add(&scene_node->events.destroy, &node_descriptor->destroy);
	scene_node->data = node_descriptor;
}

struct view *
node_view_from_node(struct wlr_scene_node *wlr_scene_node)
{
	assert(wlr_scene_node->data);
	struct node_descriptor *node_descriptor = wlr_scene_node->data;
	assert(node_descriptor->type == LAB_NODE_DESC_VIEW
		|| node_descriptor->type == LAB_NODE_DESC_XDG_POPUP);
	return (struct view *)node_descriptor->data;
}

struct lab_layer_surface *
node_layer_surface_from_node(struct wlr_scene_node *wlr_scene_node)
{
	assert(wlr_scene_node->data);
	struct node_descriptor *node_descriptor = wlr_scene_node->data;
	assert(node_descriptor->type == LAB_NODE_DESC_LAYER_SURFACE);
	return (struct lab_layer_surface *)node_descriptor->data;
}

struct lab_layer_popup *
node_layer_popup_from_node(struct wlr_scene_node *wlr_scene_node)
{
	assert(wlr_scene_node->data);
	struct node_descriptor *node_descriptor = wlr_scene_node->data;
	assert(node_descriptor->type == LAB_NODE_DESC_LAYER_POPUP);
	return (struct lab_layer_popup *)node_descriptor->data;
}

struct menuitem *
node_menuitem_from_node(struct wlr_scene_node *wlr_scene_node)
{
	assert(wlr_scene_node->data);
	struct node_descriptor *node_descriptor = wlr_scene_node->data;
	assert(node_descriptor->type == LAB_NODE_DESC_MENUITEM);
	return (struct menuitem *)node_descriptor->data;
}

struct ssd_button *
node_ssd_button_from_node(struct wlr_scene_node *wlr_scene_node)
{
	assert(wlr_scene_node->data);
	struct node_descriptor *node_descriptor = wlr_scene_node->data;
	assert(node_descriptor->type == LAB_NODE_DESC_SSD_BUTTON);
	return (struct ssd_button *)node_descriptor->data;
}

struct ssd_rounded *
node_ssd_rounded_from_node(struct wlr_scene_node *wlr_scene_node)
{
	assert(wlr_scene_node->data);
	struct node_descriptor *node_descriptor = wlr_scene_node->data;
	assert(node_descriptor->type == LAB_NODE_DESC_SSD_ROUNDED);
	return (struct ssd_rounded *)node_descriptor->data;
}
