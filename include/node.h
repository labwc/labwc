/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_NODE_DESCRIPTOR_H
#define LABWC_NODE_DESCRIPTOR_H

#include <wayland-server-core.h>
#include "common/node-type.h"

struct wlr_scene_node;

struct node_descriptor {
	enum lab_node_type type;
	struct view *view;
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
 * @view: associated view
 * @data: struct to point to as follows:
 *   - LAB_NODE_CYCLE_OSD_ITEM struct cycle_osd_item
 *   - LAB_NODE_LAYER_SURFACE  struct lab_layer_surface
 *   - LAB_NODE_LAYER_POPUP    struct lab_layer_popup
 *   - LAB_NODE_MENUITEM       struct menuitem
 *   - LAB_NODE_BUTTON_*       struct ssd_button
 */
void node_descriptor_create(struct wlr_scene_node *scene_node,
	enum lab_node_type type, struct view *view, void *data);

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
 * node_menuitem_from_node - return menuitem struct from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct menuitem *node_menuitem_from_node(
	struct wlr_scene_node *wlr_scene_node);

/**
 * node_cycle_osd_item_from_node - return cycle OSD item struct from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct cycle_osd_item *node_cycle_osd_item_from_node(
	struct wlr_scene_node *wlr_scene_node);

/**
 * node_try_ssd_button_from_node - return ssd_button or NULL from node
 * @wlr_scene_node: wlr_scene_node from which to return data
 */
struct ssd_button *node_try_ssd_button_from_node(
	struct wlr_scene_node *wlr_scene_node);

#endif /* LABWC_NODE_DESCRIPTOR_H */
