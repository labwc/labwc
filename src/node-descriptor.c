// SPDX-License-Identifier: GPL-2.0-only
#include <stdlib.h>
#include "node-descriptor.h"

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
node_descriptor_create(struct wlr_scene_node *node,
		enum node_descriptor_type type, void *data)
{
	struct node_descriptor *node_descriptor =
		calloc(1, sizeof(struct node_descriptor));
	if (!node_descriptor) {
		return;
	}
	node_descriptor->type = type;
	node_descriptor->data = data;
	node_descriptor->destroy.notify = destroy_notify;
	wl_signal_add(&node->events.destroy, &node_descriptor->destroy);
	node->data = node_descriptor;
}
