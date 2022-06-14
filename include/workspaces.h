/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_WORKSPACES_H
#define __LABWC_WORKSPACES_H

struct seat;
struct view;
struct server;
struct wl_list;

/* Double use: as config in config/rcxml.c and as instance in workspaces.c */
struct workspace {
	struct wl_list link; /* struct server.workspaces
	                        struct rcxml.workspace_config.workspaces */
	struct server *server;

	char *name;
	struct wlr_scene_tree *tree;
};


void workspaces_init(struct server *server);
void workspaces_switch_to(struct workspace *target);
void workspaces_send_to(struct view *view, struct workspace *target);
void workspaces_destroy(struct server *server);
void workspaces_osd_hide(struct seat *seat);
struct workspace * workspaces_find(struct workspace *anchor, const char *name);

#endif /* __LABWC_WORKSPACES_H */
