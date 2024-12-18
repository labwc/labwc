/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_NODE_DESCRIPTOR_H
#define LABWC_NODE_DESCRIPTOR_H
#include <wlr/types/wlr_scene.h>

struct view;
struct lab_layer_surface;
struct lab_layer_popup;
struct menuitem;
struct ssd_button;
struct scaled_scene_buffer;

enum node_descriptor_type {
	LAB_NODE_DESC_NODE = 0,
	LAB_NODE_DESC_VIEW,
	LAB_NODE_DESC_XDG_POPUP,
	LAB_NODE_DESC_LAYER_SURFACE,
	LAB_NODE_DESC_LAYER_POPUP,
	LAB_NODE_DESC_SESSION_LOCK_SURFACE,
	LAB_NODE_DESC_IME_POPUP,
	LAB_NODE_DESC_MENUITEM,
	LAB_NODE_DESC_TREE,
	LAB_NODE_DESC_SCALED_SCENE_BUFFER,
	LAB_NODE_DESC_SSD_BUTTON,
};

struct node_descriptor {
	enum node_descriptor_type type;
	void *data;
	struct wl_listener destroy;
};

/**
 * node_descriptor_create - create node descriptor for wlr_scene_node user_data
 *
 * The node_descriptor will be destroyed automatically
 * once the scene_node it is attached to is destroyed.
 *
 * @scene_node: wlr_scene_node to attached node_descriptor to
 * @type: node descriptor type
 * @data: struct to point to as follows:
 *   - LAB_NODE_DESC_VIEW           struct view
 *   - LAB_NODE_DESC_XDG_POPUP      struct view
 *   - LAB_NODE_DESC_LAYER_SURFACE  struct lab_layer_surface
 *   - LAB_NODE_DESC_LAYER_POPUP    struct lab_layer_popup
 *   - LAB_NODE_DESC_MENUITEM       struct menuitem
 *   - LAB_NODE_DESC_SSD_BUTTON     struct ssd_button
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

/**
 * node_menuitem_from_node - return menuitem struct from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct menuitem *node_menuitem_from_node(
	struct wlr_scene_node *wlr_scene_node);

/**
 * node_ssd_button_from_node - return ssd_button struct from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct ssd_button *node_ssd_button_from_node(
	struct wlr_scene_node *wlr_scene_node);

/**
 * node_scaled_scene_buffer_from_node - return scaled_scene_buffer from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct scaled_scene_buffer *node_scaled_scene_buffer_from_node(
	struct wlr_scene_node *wlr_scene_node);

#endif /* LABWC_NODE_DESCRIPTOR_H */
