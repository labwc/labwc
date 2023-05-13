/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DND_H
#define LABWC_DND_H

#include <wayland-server-core.h>

struct seat;
struct wlr_drag_icon;
struct wlr_scene_tree;

struct drag_icon {
	struct wlr_scene_tree *icon_tree;
	struct wlr_drag_icon *icon;
	struct {
		struct wl_listener map;
		struct wl_listener commit;
		struct wl_listener unmap;
		struct wl_listener destroy;
	} events;
};

void dnd_init(struct seat *seat);
void dnd_icons_show(struct seat *seat, bool show);
void dnd_icons_move(struct seat *seat, double x, double y);
void dnd_finish(struct seat *seat);

#endif /* LABWC_DND_H */
