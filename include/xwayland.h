/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_XWAYLAND_H
#define LABWC_XWAYLAND_H
#include "config.h"

#if HAVE_XWAYLAND
#include "view.h"

struct wlr_compositor;
struct wlr_output;
struct wlr_output_layout;

struct xwayland_unmanaged {
	struct server *server;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wlr_scene_node *node;
	struct wl_list link;

	struct mappable mappable;

	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener grab_focus;
	struct wl_listener request_activate;
	struct wl_listener request_configure;
/*	struct wl_listener request_fullscreen; */
	struct wl_listener set_geometry;
	struct wl_listener destroy;
	struct wl_listener set_override_redirect;

	/*
	 * True if the surface has performed a keyboard grab. labwc
	 * honors keyboard grabs and will give the surface focus when
	 * it's mapped (which may occur slightly later) and on top.
	 */
	bool ever_grabbed_focus;
};

struct xwayland_view {
	struct view base;
	struct wlr_xwayland_surface *xwayland_surface;
	bool focused_before_map;

	/* Events unique to XWayland views */
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener request_activate;
	struct wl_listener request_configure;
	struct wl_listener set_class;
	struct wl_listener set_decorations;
	struct wl_listener set_override_redirect;
	struct wl_listener set_strut_partial;
	struct wl_listener set_window_type;
	struct wl_listener focus_in;
	struct wl_listener map_request;

	/* Not (yet) implemented */
/*	struct wl_listener set_role; */
/*	struct wl_listener set_hints; */
};

void xwayland_unmanaged_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

void xwayland_view_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

struct wlr_xwayland_surface *xwayland_surface_from_view(struct view *view);

void xwayland_server_init(struct server *server,
	struct wlr_compositor *compositor);
void xwayland_server_finish(struct server *server);

void xwayland_adjust_usable_area(struct view *view,
	struct wlr_output_layout *layout, struct wlr_output *output,
	struct wlr_box *usable);

void xwayland_update_workarea(struct server *server);

void xwayland_reset_cursor(struct server *server);

#endif /* HAVE_XWAYLAND */
#endif /* LABWC_XWAYLAND_H */
