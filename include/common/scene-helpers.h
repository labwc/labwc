/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCENE_HELPERS_H
#define LABWC_SCENE_HELPERS_H

struct wlr_scene_node;
struct wlr_scene_rect;
struct wlr_scene_tree;
struct wlr_surface;

struct wlr_scene_rect *lab_wlr_scene_get_rect(struct wlr_scene_node *node);
struct wlr_scene_tree *lab_scene_tree_from_node(struct wlr_scene_node *node);
struct wlr_surface *lab_wlr_surface_from_node(struct wlr_scene_node *node);

/**
 * lab_get_prev_node - return previous (sibling) node
 * @node: node to find the previous node from
 * Return NULL if previous link is list-head which means node is bottom-most
 */
struct wlr_scene_node *lab_wlr_scene_get_prev_node(struct wlr_scene_node *node);

#endif /* LABWC_SCENE_HELPERS_H */
