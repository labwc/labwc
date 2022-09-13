// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 the sway authors
 *
 * This file is only needed in support of
 *	- unconstraining XDG popups
 *	- keeping non-layer-shell xdg-popups outside the layers.c code
 */

#include "labwc.h"
#include "node.h"

struct xdg_popup {
	struct view *parent_view;
	struct wlr_xdg_popup *wlr_popup;

	struct wl_listener destroy;
	struct wl_listener new_popup;
};

static void
popup_unconstrain(struct view *view, struct wlr_xdg_popup *popup)
{
	struct server *server = view->server;
	struct wlr_box *popup_box = &popup->current.geometry;
	struct wlr_output_layout *output_layout = server->output_layout;
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		output_layout, view->x + popup_box->x, view->y + popup_box->y);

	struct wlr_box output_box;
	wlr_output_layout_get_box(output_layout, wlr_output, &output_box);

	struct wlr_box output_toplevel_box = {
		.x = output_box.x - view->x,
		.y = output_box.y - view->y,
		.width = output_box.width,
		.height = output_box.height,
	};
	wlr_xdg_popup_unconstrain_from_box(popup, &output_toplevel_box);
}

static void
handle_xdg_popup_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->new_popup.link);
	free(popup);
}

static void
popup_handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct xdg_popup *popup = wl_container_of(listener, popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	xdg_popup_create(popup->parent_view, wlr_popup);
}

void
xdg_popup_create(struct view *view, struct wlr_xdg_popup *wlr_popup)
{
	struct wlr_xdg_surface *parent =
		wlr_surface_is_xdg_surface(wlr_popup->parent) ?
		wlr_xdg_surface_from_wlr_surface(wlr_popup->parent) : NULL;
	if (!parent) {
		wlr_log(WLR_ERROR, "parent is not a valid XDG surface");
		return;
	}

	struct xdg_popup *popup = calloc(1, sizeof(struct xdg_popup));
	if (!popup) {
		return;
	}

	popup->parent_view = view;
	popup->wlr_popup = wlr_popup;

	popup->destroy.notify = handle_xdg_popup_destroy;
	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
	popup->new_popup.notify = popup_handle_new_xdg_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

	/*
	 * We must add xdg popups to the scene graph so they get rendered. The
	 * wlroots scene graph provides a helper for this, but to use it we must
	 * provide the proper parent scene node of the xdg popup. To enable
	 * this, we always set the user data field of xdg_surfaces to the
	 * corresponding scene node.
	 */
	struct wlr_scene_tree *parent_tree = parent->surface->data;
	wlr_popup->base->surface->data =
		wlr_scene_xdg_surface_create(parent_tree, wlr_popup->base);
	node_descriptor_create(wlr_popup->base->surface->data,
		LAB_NODE_DESC_XDG_POPUP, view);

	popup_unconstrain(view, wlr_popup);
}
