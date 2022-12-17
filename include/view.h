/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_VIEW_H
#define __LABWC_VIEW_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>
#include <wlr/util/box.h>

/*
 * In labwc, a view is a container for surfaces which can be moved around by
 * the user. In practice this means XDG toplevel and XWayland windows.
 */

enum view_type {
	LAB_XDG_SHELL_VIEW,
#if HAVE_XWAYLAND
	LAB_XWAYLAND_VIEW,
#endif
};

struct view;
struct view_impl {
	void (*configure)(struct view *view, struct wlr_box geo);
	void (*close)(struct view *view);
	const char *(*get_string_prop)(struct view *view, const char *prop);
	void (*map)(struct view *view);
	void (*move)(struct view *view, int x, int y);
	void (*set_activated)(struct view *view, bool activated);
	void (*set_fullscreen)(struct view *view, bool fullscreen);
	void (*unmap)(struct view *view);
	void (*maximize)(struct view *view, bool maximize);
};

struct view {
	struct server *server;
	enum view_type type;
	const struct view_impl *impl;
	struct wl_list link;
	struct output *output;
	struct workspace *workspace;
	struct wlr_surface *surface;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_node *scene_node;

	bool mapped;
	bool been_mapped;
	bool ssd_enabled;
	bool minimized;
	bool maximized;
	uint32_t tiled;  /* private, enum view_edge in src/view.c */
	struct wlr_output *fullscreen;

	/* geometry of the wlr_surface contained within the view */
	int x, y, w, h;

	/* user defined geometry before maximize / tiling / fullscreen */
	struct wlr_box natural_geometry;

	struct view_pending_move_resize {
		bool update_x, update_y;
		int x, y;
		uint32_t width, height;
		uint32_t configure_serial;
	} pending_move_resize;

	struct ssd *ssd;

	struct wlr_foreign_toplevel_handle_v1 *toplevel_handle;
	struct wl_listener toplevel_handle_request_maximize;
	struct wl_listener toplevel_handle_request_minimize;
	struct wl_listener toplevel_handle_request_fullscreen;
	struct wl_listener toplevel_handle_request_activate;
	struct wl_listener toplevel_handle_request_close;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener surface_destroy;
	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_activate;
	struct wl_listener request_minimize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener set_title;
};

struct xdg_toplevel_view {
	struct view base;
	struct wlr_xdg_surface *xdg_surface;

	/* Events unique to xdg-toplevel views */
	struct wl_listener set_app_id;
	struct wl_listener new_popup;
};

#if HAVE_XWAYLAND
struct xwayland_view {
	struct view base;
	struct wlr_xwayland_surface *xwayland_surface;

	/* Events unique to XWayland views */
	struct wl_listener request_configure;
	struct wl_listener set_app_id;		/* TODO: s/set_app_id/class/ */
	struct wl_listener set_decorations;
	struct wl_listener override_redirect;

	/* Not (yet) implemented */
/*	struct wl_listener set_role; */
/*	struct wl_listener set_window_type; */
/*	struct wl_listener set_hints; */
};
#endif

void view_set_activated(struct view *view);
void view_close(struct view *view);

/**
 * view_move_resize - resize and move view
 * @view: view to be resized and moved
 * @geo: the new geometry
 * NOTE: Only use this when the view actually changes width and/or height
 * otherwise the serials might cause a delay in moving xdg-shell clients.
 * For move only, use view_move()
 */
void view_move_resize(struct view *view, struct wlr_box geo);
void view_move(struct view *view, int x, int y);
void view_moved(struct view *view);
void view_minimize(struct view *view, bool minimized);
/* view_wlr_output - return the output that a view is mostly on */
struct wlr_output *view_wlr_output(struct view *view);
void view_store_natural_geometry(struct view *view);
void view_center(struct view *view);
void view_restore_to(struct view *view, struct wlr_box geometry);
void view_set_untiled(struct view *view);
void view_maximize(struct view *view, bool maximize,
	bool store_natural_geometry);
void view_set_fullscreen(struct view *view, bool fullscreen,
	struct wlr_output *wlr_output);
void view_toggle_maximize(struct view *view);
void view_toggle_decorations(struct view *view);
void view_toggle_always_on_top(struct view *view);
void view_move_to_workspace(struct view *view, struct workspace *workspace);
void view_set_decorations(struct view *view, bool decorations);
void view_toggle_fullscreen(struct view *view);
void view_adjust_for_layout_change(struct view *view);
void view_discover_output(struct view *view);
void view_move_to_edge(struct view *view, const char *direction);
void view_snap_to_edge(struct view *view, const char *direction,
	bool store_natural_geometry);
const char *view_get_string_prop(struct view *view, const char *prop);
void view_update_title(struct view *view);
void view_update_app_id(struct view *view);
void view_reload_ssd(struct view *view);

void view_impl_map(struct view *view);
void view_adjust_size(struct view *view, int *w, int *h);

bool view_compute_centered_position(struct view *view, int w, int h,
	int *x, int *y);

void view_on_output_destroy(struct view *view);
void view_destroy(struct view *view);

/* xdg.c */
struct wlr_xdg_surface *xdg_surface_from_view(struct view *view);

/* xwayland.c */
#if HAVE_XWAYLAND
struct wlr_xwayland_surface *xwayland_surface_from_view(struct view *view);
#endif

#endif /* __LABWC_VIEW_H */
