#ifndef __LABWC_LAYERS_H
#define __LABWC_LAYERS_H
#include <wayland-server.h>
#include <wlr/types/wlr_layer_shell_v1.h>

struct server;

struct lab_layer_surface {
	struct wlr_layer_surface_v1 *layer_surface;
	struct server *server;
	struct wl_list link;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener output_destroy;

	struct wlr_box geo;
	bool mapped;
};

void layers_init(struct server *server);

#endif /* __LABWC_LAYERS_H */
