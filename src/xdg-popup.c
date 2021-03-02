/*
 * Copyright (C) 2020 the sway authors
 * This file is only needed in support of tracking damage
 */

#include "labwc.h"

static void
xdg_popup_destroy(struct view_child *view_child)
{
	if (!view_child) {
		return;
	}
	struct xdg_popup *popup = (struct xdg_popup *)view_child;
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->map.link);
	wl_list_remove(&popup->unmap.link);
	wl_list_remove(&popup->new_popup.link);
	view_child_finish(&popup->view_child);
	free(popup);
}

static void
handle_xdg_popup_map(struct wl_listener *listener, void *data)
{
	struct xdg_popup *popup = wl_container_of(listener, popup, map);
	damage_view_whole(popup->view_child.parent);
}

static void
handle_xdg_popup_unmap(struct wl_listener *listener, void *data)
{
	struct xdg_popup *popup = wl_container_of(listener, popup, unmap);
	damage_view_whole(popup->view_child.parent);
}

static void
handle_xdg_popup_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_popup *popup = wl_container_of(listener, popup, destroy);
	struct view_child *view_child = (struct view_child *)popup;
	xdg_popup_destroy(view_child);
}

static void
popup_handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct xdg_popup *popup = wl_container_of(listener, popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	xdg_popup_create(popup->view_child.parent, wlr_popup);
}

void
xdg_popup_create(struct view *view, struct wlr_xdg_popup *wlr_popup)
{
	struct xdg_popup *popup = calloc(1, sizeof(struct xdg_popup));
	if (!popup) {
		return;
	}

	popup->wlr_popup = wlr_popup;
	view_child_init(&popup->view_child, view, wlr_popup->base->surface);

	popup->destroy.notify = handle_xdg_popup_destroy;
	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
	popup->map.notify = handle_xdg_popup_map;
	wl_signal_add(&wlr_popup->base->events.map, &popup->map);
	popup->unmap.notify = handle_xdg_popup_unmap;
	wl_signal_add(&wlr_popup->base->events.unmap, &popup->unmap);
	popup->new_popup.notify = popup_handle_new_xdg_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

	/* TODO: popup_unconstrain() */
}
