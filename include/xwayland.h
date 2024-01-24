/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_XWAYLAND_H
#define LABWC_XWAYLAND_H
#include "config.h"

#if HAVE_XWAYLAND
#include <assert.h>
#include <stdbool.h>
#include <xcb/xcb.h>
#include "common/macros.h"
#include "view.h"

struct wlr_compositor;
struct wlr_output;
struct wlr_output_layout;

enum atom {
	/* https://specifications.freedesktop.org/wm-spec/wm-spec-1.4.html#idm45649101374512 */
	NET_WM_WINDOW_TYPE_DESKTOP = 0,
	NET_WM_WINDOW_TYPE_DOCK,
	NET_WM_WINDOW_TYPE_TOOLBAR,
	NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_UTILITY,
	NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_DIALOG,
	NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	NET_WM_WINDOW_TYPE_POPUP_MENU,
	NET_WM_WINDOW_TYPE_TOOLTIP,
	NET_WM_WINDOW_TYPE_NOTIFICATION,
	NET_WM_WINDOW_TYPE_COMBO,
	NET_WM_WINDOW_TYPE_DND,
	NET_WM_WINDOW_TYPE_NORMAL,

	ATOM_LEN
};

static const char * const atom_names[] = {
	"_NET_WM_WINDOW_TYPE_DESKTOP",
	"_NET_WM_WINDOW_TYPE_DOCK",
	"_NET_WM_WINDOW_TYPE_TOOLBAR",
	"_NET_WM_WINDOW_TYPE_MENU",
	"_NET_WM_WINDOW_TYPE_UTILITY",
	"_NET_WM_WINDOW_TYPE_SPLASH",
	"_NET_WM_WINDOW_TYPE_DIALOG",
	"_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	"_NET_WM_WINDOW_TYPE_POPUP_MENU",
	"_NET_WM_WINDOW_TYPE_TOOLTIP",
	"_NET_WM_WINDOW_TYPE_NOTIFICATION",
	"_NET_WM_WINDOW_TYPE_COMBO",
	"_NET_WM_WINDOW_TYPE_DND",
	"_NET_WM_WINDOW_TYPE_NORMAL",
};

static_assert(
	ARRAY_SIZE(atom_names) == ATOM_LEN,
	"Xwayland atoms out of sync");

extern xcb_atom_t atoms[ATOM_LEN];

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
	struct wl_listener set_strut_partial;
	struct wl_listener set_window_type;

	/* Not (yet) implemented */
/*	struct wl_listener set_role; */
/*	struct wl_listener set_hints; */
};

void xwayland_unmanaged_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

void xwayland_view_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

void xwayland_adjust_stacking_order(struct server *server);

struct wlr_xwayland_surface *xwayland_surface_from_view(struct view *view);

bool xwayland_surface_contains_window_type(
	struct wlr_xwayland_surface *surface, enum atom window_type);

void xwayland_server_init(struct server *server,
	struct wlr_compositor *compositor);
void xwayland_server_finish(struct server *server);

void xwayland_adjust_usable_area(struct view *view,
	struct wlr_output_layout *layout, struct wlr_output *output,
	struct wlr_box *usable);

void xwayland_update_workarea(struct server *server);

#endif /* HAVE_XWAYLAND */
#endif /* LABWC_XWAYLAND_H */
