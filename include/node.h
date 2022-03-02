/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_NODE_DESCRIPTOR_H
#define __LABWC_NODE_DESCRIPTOR_H
#include <wlr/types/wlr_scene.h>

enum node_descriptor_type {
	LAB_NODE_DESC_NODE = 0,
	LAB_NODE_DESC_VIEW,		/* *data --> struct view */
	LAB_NODE_DESC_XDG_POPUP,	/* *data --> struct view */
	LAB_NODE_DESC_LAYER_SURFACE,	/* *data --> struct lab_layer_surface */
	LAB_NODE_DESC_LAYER_POPUP,	/* *data --> struct lab_layer_popup */
};

struct node_descriptor {
	enum node_descriptor_type type;
	void *data;
	struct wl_listener destroy;
};

void node_descriptor_create(struct wlr_scene_node *node,
	enum node_descriptor_type type, void *data);

#endif /* __LABWC_NODE_DESCRIPTOR_H */
