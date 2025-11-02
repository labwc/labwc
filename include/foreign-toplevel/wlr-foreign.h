/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_WLR_FOREIGN_TOPLEVEL_H
#define LABWC_WLR_FOREIGN_TOPLEVEL_H

#include <wayland-server-core.h>

struct wlr_foreign_toplevel {
	struct view *view;
	struct wlr_foreign_toplevel_handle_v1 *handle;

	/* Client side events */
	struct {
		struct wl_listener request_maximize;
		struct wl_listener request_minimize;
		struct wl_listener request_fullscreen;
		struct wl_listener request_activate;
		struct wl_listener request_close;
		struct wl_listener handle_destroy;
	} on;

	/* Compositor side state updates */
	struct {
		struct wl_listener new_app_id;
		struct wl_listener new_title;
		struct wl_listener new_outputs;
		struct wl_listener maximized;
		struct wl_listener minimized;
		struct wl_listener fullscreened;
		struct wl_listener activated;
	} on_view;
};

void wlr_foreign_toplevel_init(struct wlr_foreign_toplevel *wlr_toplevel,
	struct view *view);
void wlr_foreign_toplevel_set_parent(struct wlr_foreign_toplevel *wlr_toplevel,
	struct wlr_foreign_toplevel *parent);
void wlr_foreign_toplevel_finish(struct wlr_foreign_toplevel *wlr_toplevel);
void wlr_foreign_toplevel_refresh_outputs(struct wlr_foreign_toplevel *wlr_toplevel);

#endif /* LABWC_WLR_FOREIGN_TOPLEVEL_H */
