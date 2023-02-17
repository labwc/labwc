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
};

static void
unmanaged_handle_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_unmanaged *unmanaged = wl_container_of(listener, unmanaged, destroy);
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

	unmanaged->tree = server->view_tree_always_on_top;
	unmanaged->node = &wlr_scene_surface_create(unmanaged->tree,
		wlr_xdg_surface->surface)->buffer->node;
	node_descriptor_create(unmanaged->node, LAB_NODE_DESC_XDG_UNMANAGED, unmanaged);
	wlr_scene_node_set_position(unmanaged->node, 0, 0);
	cursor_update_focus(unmanaged->server);
}
