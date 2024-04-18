// SPDX-License-Identifier: GPL-2.0-only
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include "common/mem.h"
#include "decorations.h"
#include "labwc.h"
#include "view.h"

struct xdg_deco {
	struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration;
	struct view *view;
	struct wl_listener destroy;
	struct wl_listener request_mode;
};

static void
xdg_deco_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_deco *xdg_deco = wl_container_of(listener, xdg_deco, destroy);
	wl_list_remove(&xdg_deco->destroy.link);
	wl_list_remove(&xdg_deco->request_mode.link);
	free(xdg_deco);
}

static void
xdg_deco_request_mode(struct wl_listener *listener, void *data)
{
	struct xdg_deco *xdg_deco = wl_container_of(listener, xdg_deco, request_mode);
	enum wlr_xdg_toplevel_decoration_v1_mode client_mode =
		xdg_deco->wlr_xdg_decoration->requested_mode;

	switch (client_mode) {
	case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
		xdg_deco->view->ssd_preference = LAB_SSD_PREF_SERVER;
		break;
	case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
		xdg_deco->view->ssd_preference = LAB_SSD_PREF_CLIENT;
		break;
	case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE:
		xdg_deco->view->ssd_preference = LAB_SSD_PREF_UNSPEC;
		client_mode = rc.xdg_shell_server_side_deco
			? WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
			: WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
		break;
	default:
		wlr_log(WLR_ERROR, "Unspecified xdg decoration variant "
			"requested: %u", client_mode);
	}

	wlr_xdg_toplevel_decoration_v1_set_mode(xdg_deco->wlr_xdg_decoration,
		client_mode);
	if (client_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
		view_set_ssd_mode(xdg_deco->view, LAB_SSD_MODE_FULL);
	} else {
		view_set_ssd_mode(xdg_deco->view, LAB_SSD_MODE_NONE);
	}
}

static void
xdg_toplevel_decoration(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration = data;
	struct wlr_xdg_surface *xdg_surface = wlr_xdg_decoration->toplevel->base;
	if (!xdg_surface || !xdg_surface->data) {
		wlr_log(WLR_ERROR,
			"Invalid surface supplied for xdg decorations");
		return;
	}

	struct xdg_deco *xdg_deco = znew(*xdg_deco);
	xdg_deco->wlr_xdg_decoration = wlr_xdg_decoration;
	xdg_deco->view = (struct view *)xdg_surface->data;

	wl_signal_add(&wlr_xdg_decoration->events.destroy, &xdg_deco->destroy);
	xdg_deco->destroy.notify = xdg_deco_destroy;

	wl_signal_add(&wlr_xdg_decoration->events.request_mode,
		&xdg_deco->request_mode);
	xdg_deco->request_mode.notify = xdg_deco_request_mode;

	xdg_deco_request_mode(&xdg_deco->request_mode, wlr_xdg_decoration);
}

void
xdg_server_decoration_init(struct server *server)
{
	struct wlr_xdg_decoration_manager_v1 *xdg_deco_mgr = NULL;
	xdg_deco_mgr = wlr_xdg_decoration_manager_v1_create(server->wl_display);
	if (!xdg_deco_mgr) {
		wlr_log(WLR_ERROR, "unable to create the XDG deco manager");
		exit(EXIT_FAILURE);
	}

	wl_signal_add(&xdg_deco_mgr->events.new_toplevel_decoration,
		&server->xdg_toplevel_decoration);
	server->xdg_toplevel_decoration.notify = xdg_toplevel_decoration;
}
