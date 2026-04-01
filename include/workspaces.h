/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_WORKSPACES_H
#define LABWC_WORKSPACES_H

#include <stdbool.h>
#include <wayland-util.h>
#include <wayland-server-core.h>

struct seat;
struct server;
struct wlr_scene_tree;

struct workspace {
	struct wl_list link; /* struct server.workspaces */

	char *name;
	struct wlr_scene_tree *tree;
	struct wlr_scene_tree *view_trees[3];

	struct wlr_ext_workspace_handle_v1 *ext_workspace;
};

void workspaces_init(void);
void workspaces_switch_to(struct workspace *target, bool update_focus);
void workspaces_destroy(void);
void workspaces_osd_hide(struct seat *seat);
struct workspace *workspaces_find(struct workspace *anchor, const char *name,
	bool wrap);
void workspaces_reconfigure(void);

#endif /* LABWC_WORKSPACES_H */
