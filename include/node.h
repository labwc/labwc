/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_NODE_DESCRIPTOR_H
#define __LABWC_NODE_DESCRIPTOR_H
#include <wlr/types/wlr_scene.h>

struct view;
struct lab_layer_surface;
struct lab_layer_popup;

enum node_descriptor_type {
	LAB_NODE_DESC_NODE = 0,
	LAB_NODE_DESC_VIEW,
	LAB_NODE_DESC_XDG_POPUP,
	LAB_NODE_DESC_LAYER_SURFACE,
	LAB_NODE_DESC_LAYER_POPUP,
};

struct node_descriptor {
	enum node_descriptor_type type;
	void *data;
	struct wl_listener destroy;
};

/**
 * node_descriptor_create - create node descriptor for wlr_scene_node user_data
 * @scene_node: wlr_scene_node to attached node_descriptor to
 * @type: node descriptor type
 * @data: struct to point to as follows:
 *   - LAB_NODE_DESC_VIEW		struct view
 *   - LAB_NODE_DESC_XDG_POPUP		struct view
 *   - LAB_NODE_DESC_LAYER_SURFACE	struct lab_layer_surface
 *   - LAB_NODE_DESC_LAYER_POPUP	struct lab_layer_popup
 */
void node_descriptor_create(struct wlr_scene_node *scene_node,
	enum node_descriptor_type type, void *data);

/**
 * node_view_from_node - return view struct from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct view *node_view_from_node(struct wlr_scene_node *wlr_scene_node);

/**
 * node_lab_surface_from_node - return lab_layer_surface struct from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct lab_layer_surface *node_layer_surface_from_node(
	struct wlr_scene_node *wlr_scene_node);

/**
 * node_layer_popup_from_node - return lab_layer_popup struct from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct lab_layer_popup *node_layer_popup_from_node(
	struct wlr_scene_node *wlr_scene_node);

#endif /* __LABWC_NODE_DESCRIPTOR_H */
