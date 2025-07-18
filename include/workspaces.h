/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_WORKSPACES_H
#define LABWC_WORKSPACES_H

#include <stdbool.h>
#include <wayland-util.h>
#include <wayland-server-core.h>

struct seat;
struct server;
struct wlr_scene_tree;

/* Double use: as config in config/rcxml.c and as instance in workspaces.c */
struct workspace {
	struct wl_list link; /*
			      * struct server.workspaces
			      * struct rcxml.workspace_config.workspaces
			      */
	struct server *server;

	char *name;
	struct wlr_scene_tree *tree;

	struct wlr_ext_workspace_handle_v1 *ext_workspace;
	struct {
		struct wl_listener activate;
		struct wl_listener deactivate;
		struct wl_listener assign;
		struct wl_listener remove;
	} on_ext;
};

void workspaces_init(struct server *server);
void workspaces_switch_to(struct workspace *target, bool update_focus);
void workspaces_destroy(struct server *server);
void workspaces_osd_hide(struct seat *seat);
struct workspace *workspaces_find(struct workspace *anchor, const char *name,
	bool wrap);
void workspaces_reconfigure(struct server *server);

#endif /* LABWC_WORKSPACES_H */
