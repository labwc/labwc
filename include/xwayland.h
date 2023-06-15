/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_XWAYLAND_H
#define LABWC_XWAYLAND_H
#include "config.h"
#if HAVE_XWAYLAND
#include "view.h"

struct wlr_compositor;

struct xwayland_unmanaged {
	struct server *server;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wlr_scene_node *node;
	struct wl_list link;

	struct mappable mappable;

	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener request_activate;
	struct wl_listener request_configure;
/*	struct wl_listener request_fullscreen; */
	struct wl_listener set_geometry;
	struct wl_listener destroy;
	struct wl_listener set_override_redirect;
};

struct xwayland_view {
	struct view base;
	struct wlr_xwayland_surface *xwayland_surface;

	/* Events unique to XWayland views */
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener request_activate;
	struct wl_listener request_configure;
	struct wl_listener set_class;
	struct wl_listener set_decorations;
	struct wl_listener set_override_redirect;

	/* Not (yet) implemented */
/*	struct wl_listener set_role; */
/*	struct wl_listener set_window_type; */
/*	struct wl_listener set_hints; */
};

void xwayland_unmanaged_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

void xwayland_view_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

void xwayland_adjust_stacking_order(struct server *server);

struct wlr_xwayland_surface *xwayland_surface_from_view(struct view *view);

void xwayland_server_init(struct server *server,
	struct wlr_compositor *compositor);
void xwayland_server_finish(struct server *server);

#endif /* HAVE_XWAYLAND */
#endif /* LABWC_XWAYLAND_H */
