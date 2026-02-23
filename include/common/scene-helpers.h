/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCENE_HELPERS_H
#define LABWC_SCENE_HELPERS_H

#include <stdbool.h>

struct wlr_buffer;
struct wlr_scene_node;
struct wlr_surface;
struct wlr_scene_output;
struct wlr_output_state;

struct wlr_surface *lab_wlr_surface_from_node(struct wlr_scene_node *node);

/* variants of wlr_scene_*_create() functions that don't return NULL */
struct wlr_scene_tree *lab_wlr_scene_tree_create(struct wlr_scene_tree *parent);
struct wlr_scene_rect *lab_wlr_scene_rect_create(struct wlr_scene_tree *parent,
	int width, int height, const float color[static 4]);
struct wlr_scene_buffer *lab_wlr_scene_buffer_create(
	struct wlr_scene_tree *parent, struct wlr_buffer *buffer);

/**
 * lab_get_prev_node - return previous (sibling) node
 * @node: node to find the previous node from
 * Return NULL if previous link is list-head which means node is bottom-most
 */
struct wlr_scene_node *lab_wlr_scene_get_prev_node(struct wlr_scene_node *node);

/* A variant of wlr_scene_output_commit() that respects wlr_output->pending */
bool lab_wlr_scene_output_commit(struct wlr_scene_output *scene_output,
	struct wlr_output_state *state);

#endif /* LABWC_SCENE_HELPERS_H */
