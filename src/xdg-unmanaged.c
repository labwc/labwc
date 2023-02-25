// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include "common/mem.h"
#include "labwc.h"
#include "node.h"
#include "view.h"
#include "view-impl-common.h"
#include "workspaces.h"
#include "xdg-unmanaged.h"

struct xdg_unmanaged {
	struct server *server;
	struct wlr_xdg_surface *wlr_xdg_surface;
	struct wlr_scene_tree *tree;
	struct wlr_scene_node *node;

	struct wl_listener destroy;
	struct wl_listener new_popup;
};

struct popup {
	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_tree *scene_tree;
	struct wlr_box output_toplevel_sx_box;

	struct wl_listener destroy;
	struct wl_listener new_popup;
};

static void
popup_handle_destroy(struct wl_listener *listener, void *data)
{
	struct popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->new_popup.link);
	free(popup);
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data);

static struct popup *
create_popup(struct wlr_xdg_popup *wlr_popup, struct wlr_scene_tree *parent,
		struct wlr_box *output_toplevel_sx_box)
{
	struct popup *popup = znew(*popup);
	popup->wlr_popup = wlr_popup;
	popup->scene_tree = wlr_scene_xdg_surface_create(parent, wlr_popup->base);
	if (!popup->scene_tree) {
		free(popup);
		return NULL;
	}
	node_descriptor_create(&popup->scene_tree->node,
		LAB_NODE_DESC_LAYER_POPUP, popup);

	popup->destroy.notify = popup_handle_destroy;
	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, output_toplevel_sx_box);
	return popup;
}

static void
popup_handle_new_popup(struct wl_listener *listener, void *data)
{
	struct popup *popup = wl_container_of(listener, popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	struct popup *new_popup = create_popup(wlr_popup, popup->scene_tree,
		&popup->output_toplevel_sx_box);
	new_popup->output_toplevel_sx_box = popup->output_toplevel_sx_box;
}

static void
handle_new_popup(struct wl_listener *listener, void *data)
{
	struct xdg_unmanaged *unmanaged = wl_container_of(listener, unmanaged, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;

	int lx, ly;
	wlr_scene_node_coords(unmanaged->node, &lx, &ly);

	struct wlr_box output_box = { 0 };

	struct wlr_box output_toplevel_sx_box = {
		.x = output_box.x - lx,
		.y = output_box.y - ly,
		.width = output_box.width,
		.height = output_box.height,
	};
	struct popup *popup = create_popup(wlr_popup,
		unmanaged->tree, &output_toplevel_sx_box);
	popup->output_toplevel_sx_box = output_toplevel_sx_box;
}

static void
unmanaged_handle_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_unmanaged *unmanaged = wl_container_of(listener, unmanaged, destroy);
	wl_list_remove(&unmanaged->new_popup.link);
	wl_list_remove(&unmanaged->destroy.link);
	free(unmanaged);
}

void
xdg_unmanaged_create(struct server *server, struct wlr_xdg_surface *wlr_xdg_surface)
{
	struct xdg_unmanaged *unmanaged = znew(*unmanaged);
	unmanaged->server = server;
	unmanaged->wlr_xdg_surface = wlr_xdg_surface;

	unmanaged->destroy.notify = unmanaged_handle_destroy;
	wl_signal_add(&wlr_xdg_surface->events.destroy, &unmanaged->destroy);

	unmanaged->new_popup.notify = handle_new_popup;
	wl_signal_add(&wlr_xdg_surface->events.new_popup, &unmanaged->new_popup);

	unmanaged->tree = server->view_tree_always_on_top;
	unmanaged->node = &wlr_scene_surface_create(unmanaged->tree,
		wlr_xdg_surface->surface)->buffer->node;
	node_descriptor_create(unmanaged->node, LAB_NODE_DESC_XDG_UNMANAGED, unmanaged);
	wlr_scene_node_set_position(unmanaged->node, 0, 0);
	cursor_update_focus(unmanaged->server);
}
