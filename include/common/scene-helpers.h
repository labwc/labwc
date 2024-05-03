/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCENE_HELPERS_H
#define LABWC_SCENE_HELPERS_H

#include <stdbool.h>

struct wlr_scene_node;
struct wlr_surface;
struct wlr_scene_output;

struct wlr_surface *lab_wlr_surface_from_node(struct wlr_scene_node *node);

/**
 * lab_get_prev_node - return previous (sibling) node
 * @node: node to find the previous node from
 * Return NULL if previous link is list-head which means node is bottom-most
 */
struct wlr_scene_node *lab_wlr_scene_get_prev_node(struct wlr_scene_node *node);

/* A variant of wlr_scene_output_commit() that respects wlr_output->pending */
bool lab_wlr_scene_output_commit(struct wlr_scene_output *scene_output);

enum magnify_dir {
	MAGNIFY_INCREASE,
	MAGNIFY_DECREASE
};

void magnify_toggle(void);
void magnify_set_scale(enum magnify_dir dir);

#endif /* LABWC_SCENE_HELPERS_H */
