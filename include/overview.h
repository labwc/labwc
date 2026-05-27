/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OVERVIEW_H
#define LABWC_OVERVIEW_H

#include <stdbool.h>
#include <wayland-server-core.h>

struct output;
struct view;
struct wlr_scene_node;

struct overview_item {
	struct view *view;
	struct wlr_scene_tree *tree;
	struct wl_list link;
};

struct overview_state {
	bool active;
	struct wl_list items; /* struct overview_item.link */
	struct wlr_scene_tree *tree;
	struct output *output;
};

/* Begin overview mode */
void overview_begin(void);

/* End overview mode */
void overview_finish(bool focus_selected);

/* Toggle overview mode */
void overview_toggle(void);

/* Focus the clicked window and close overview */
void overview_on_cursor_release(struct wlr_scene_node *node);

/* Get overview item from scene node */
struct overview_item *node_overview_item_from_node(
	struct wlr_scene_node *wlr_scene_node);

#endif /* LABWC_OVERVIEW_H */
