/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_LAYERS_H
#define __LABWC_LAYERS_H
#include <wayland-server.h>
#include <wlr/types/wlr_layer_shell_v1.h>

struct server;
struct output;

#define LAB_NR_LAYERS (4)

struct lab_layer_surface {
	struct wl_list link; /* output::layers */
	struct wlr_scene_layer_surface_v1 *scene_layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener output_destroy;
	struct wl_listener new_popup;

	struct wlr_box geo;
	bool mapped;
	/* TODO: add extent? */
	struct server *server;
};

struct lab_layer_popup {
	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_node *scene_node;

	struct wl_listener destroy;
	struct wl_listener new_popup;
};

void layers_init(struct server *server);

void layers_arrange(struct output *output);

#endif /* __LABWC_LAYERS_H */
