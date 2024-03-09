/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_LAYERS_H
#define LABWC_LAYERS_H
#include <wayland-server.h>
#include <wlr/types/wlr_layer_shell_v1.h>

struct server;
struct output;
struct seat;

struct lab_layer_surface {
	struct wlr_scene_layer_surface_v1 *scene_layer_surface;
	struct server *server;

	bool mapped;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener output_destroy;
	struct wl_listener node_destroy;
	struct wl_listener new_popup;
};

struct lab_layer_popup {
	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_tree *scene_tree;

	/* To simplify moving popup nodes from the bottom to the top layer */
	struct wlr_box output_toplevel_sx_box;

	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener new_popup;
};

void layers_init(struct server *server);

void layers_arrange(struct output *output);
void layer_try_set_focus(struct seat *seat,
	struct wlr_layer_surface_v1 *layer_surface);

#endif /* LABWC_LAYERS_H */
