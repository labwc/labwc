/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_WORKSPACES_H
#define LABWC_WORKSPACES_H

#include <stdbool.h>
#include <wayland-util.h>
#include <wayland-server-core.h>

struct seat;
struct server;

struct workspace {
	struct wl_list link; /* struct server.workspaces */
	struct server *server;

	char *name;

	struct lab_cosmic_workspace *cosmic_workspace;
	struct {
		struct wl_listener activate;
		struct wl_listener deactivate;
		struct wl_listener remove;
	} on_cosmic;

	struct lab_ext_workspace *ext_workspace;
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
