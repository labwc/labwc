// SPDX-License-Identifier: GPL-2.0-only
#include "config.h"
#include <assert.h>
#include "common/list.h"
#include "common/scene-helpers.h"
#include "dnd.h"
#include "labwc.h"
#include "layers.h"
#include "node.h"
#include "ssd.h"
#include "view.h"
#include "workspaces.h"
#include "xwayland.h"

static void
move_to_front(struct view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
desktop_move_to_front(struct view *view)
{
	if (!view) {
		return;
	}
	move_to_front(view);
#if HAVE_XWAYLAND
	xwayland_move_sub_views_to_front(view, move_to_front);
#endif
	cursor_update_focus(view->server);
}

void
desktop_move_to_back(struct view *view)
{
	if (!view) {
		return;
	}
	wl_list_remove(&view->link);
	wl_list_append(&view->server->views, &view->link);
}

void
desktop_arrange_all_views(struct server *server)
{
	/* Adjust window positions/sizes */
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		view_adjust_for_layout_change(view);
	}
}

void
desktop_focus_and_activate_view(struct seat *seat, struct view *view)
{
	if (!view) {
		seat_focus_surface(seat, NULL);
		return;
	}

	/*
	 * Guard against views with no mapped surfaces when handling
	 * 'request_activate' and 'request_minimize'.
	 * See notes by isfocusable()
	 */
	if (!view->surface) {
		return;
	}

	if (input_inhibit_blocks_surface(seat, view->surface->resource)) {
		return;
	}

	if (view->minimized) {
		/*
		 * Unminimizing will map the view which triggers a call to this
		 * function again.
		 */
		view_minimize(view, false);
		return;
	}

	if (!view->mapped) {
		return;
	}

	struct wlr_surface *prev_surface;
	prev_surface = seat->seat->keyboard_state.focused_surface;

	/* Do not re-focus an already focused surface. */
	if (prev_surface == view->surface) {
		return;
	}

	view_set_activated(view);
	seat_focus_surface(seat, view->surface);
}

/*
 * Some xwayland apps produce unmapped surfaces on startup and also leave
 * some unmapped surfaces kicking around on 'close' (for example leafpad's
 * "about" dialogue). Whilst this is not normally a problem, we have to be
 * careful when cycling between views. The only views we should focus are
 * those that are already mapped and those that have been minimized.
 */
bool
isfocusable(struct view *view)
{
	/* filter out those xwayland surfaces that have never been mapped */
	if (!view->surface) {
		return false;
	}
	return (view->mapped || view->minimized);
}

static struct wl_list *
get_prev_item(struct wl_list *item)
{
	return item->prev;
}

static struct wl_list *
get_next_item(struct wl_list *item)
{
	return item->next;
}

static struct view *
first_view(struct server *server)
{
	struct wlr_scene_node *node;
	struct wl_list *list_head =
		&server->workspace_current->tree->children;
	wl_list_for_each_reverse(node, list_head, link) {
		struct view *view = node_view_from_node(node);
		if (isfocusable(view)) {
			return view;
		}
	}
	return NULL;
}

struct view *
desktop_cycle_view(struct server *server, struct view *start_view,
		enum lab_cycle_dir dir)
{
	/*
	 * Views are listed in stacking order, topmost first.  Usually
	 * the topmost view is already focused, so we pre-select the
	 * view second from the top:
	 *
	 *   View #1 (on top, currently focused)
	 *   View #2 (pre-selected)
	 *   View #3
	 *   ...
	 *
	 * This assumption doesn't always hold with XWayland views,
	 * where a main application window may be focused but an
	 * focusable sub-view (e.g. an about dialog) may still be on
	 * top of it.  In that case, we pre-select the sub-view:
	 *
	 *   Sub-view of #1 (on top, pre-selected)
	 *   Main view #1 (currently focused)
	 *   Main view #2
	 *   ...
	 *
	 * The general rule is:
	 *
	 *   - Pre-select the top view if NOT already focused
	 *   - Otherwise select the view second from the top
	 */

	/* Make sure to have all nodes in their actual ordering */
	osd_preview_restore(server);

	if (!start_view) {
		start_view = first_view(server);
		if (!start_view || start_view != desktop_focused_view(server)) {
			return start_view;  /* may be NULL */
		}
	}
	struct view *view = start_view;
	struct wlr_scene_node *node = &view->scene_tree->node;

	assert(node->parent);
	struct wl_list *list_head = &node->parent->children;
	struct wl_list *list_item = &node->link;
	struct wl_list *(*iter)(struct wl_list *list);

	/* Scene nodes are ordered like last node == displayed topmost */
	iter = dir == LAB_CYCLE_DIR_FORWARD ? get_prev_item : get_next_item;

	do {
		list_item = iter(list_item);
		if (list_item == list_head) {
			/* Start / End of list reached. Roll over */
			list_item = iter(list_item);
		}
		node = wl_container_of(list_item, node, link);
		view = node_view_from_node(node);
		if (isfocusable(view)) {
			return view;
		}
	} while (view != start_view);

	/* No focusable views found, including the one we started with */
	return NULL;
}

static struct view *
topmost_mapped_view(struct server *server)
{
	struct view *view;
	struct wl_list *node_list;
	struct wlr_scene_node *node;
	node_list = &server->workspace_current->tree->children;
	wl_list_for_each_reverse(node, node_list, link) {
		view = node_view_from_node(node);
		if (view->mapped) {
			return view;
		}
	}
	return NULL;
}

struct view *
desktop_focused_view(struct server *server)
{
	struct seat *seat = &server->seat;
	struct wlr_surface *focused_surface;
	focused_surface = seat->seat->keyboard_state.focused_surface;
	if (!focused_surface) {
		return NULL;
	}
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->surface == focused_surface) {
			return view;
		}
	}

	return NULL;
}

void
desktop_focus_topmost_mapped_view(struct server *server)
{
	struct view *view = topmost_mapped_view(server);
	desktop_focus_and_activate_view(&server->seat, view);
	desktop_move_to_front(view);
}

static struct wlr_surface *
get_surface_from_layer_node(struct wlr_scene_node *node)
{
	assert(node->data);
	struct node_descriptor *desc = (struct node_descriptor *)node->data;
	if (desc->type == LAB_NODE_DESC_LAYER_SURFACE) {
		struct lab_layer_surface *surface;
		surface = node_layer_surface_from_node(node);
		return surface->scene_layer_surface->layer_surface->surface;
	} else if (desc->type == LAB_NODE_DESC_LAYER_POPUP) {
		struct lab_layer_popup *popup;
		popup = node_layer_popup_from_node(node);
		return popup->wlr_popup->base->surface;
	}
	return NULL;
}

static bool
is_layer_descendant(struct wlr_scene_node *node)
{
	goto start;
	while (node) {
		struct node_descriptor *desc = node->data;
		if (desc && desc->type == LAB_NODE_DESC_LAYER_SURFACE) {
			return true;
		}
start:
		node = node->parent ? &node->parent->node : NULL;
	}
	return false;
}

/* TODO: make this less big and scary */
struct cursor_context
get_cursor_context(struct server *server)
{
	struct cursor_context ret = {.type = LAB_SSD_NONE};
	struct wlr_cursor *cursor = server->seat.cursor;

	/* Prevent drag icons to be on top of the hitbox detection */
	if (server->seat.drag.active) {
		dnd_icons_show(&server->seat, false);
	}

	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->scene->tree.node,
			cursor->x, cursor->y, &ret.sx, &ret.sy);

	if (server->seat.drag.active) {
		dnd_icons_show(&server->seat, true);
	}

	ret.node = node;
	if (!node) {
		ret.type = LAB_SSD_ROOT;
		return ret;
	}
#if HAVE_XWAYLAND
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_surface *surface = lab_wlr_surface_from_node(node);
		if (node->parent == server->unmanaged_tree) {
			ret.type = LAB_SSD_UNMANAGED;
			ret.surface = surface;
			return ret;
		}
	}
#endif
	while (node) {
		struct node_descriptor *desc = node->data;
		if (desc) {
			switch (desc->type) {
			case LAB_NODE_DESC_VIEW:
			case LAB_NODE_DESC_XDG_POPUP:
				ret.view = desc->data;
				ret.type = ssd_get_part_type(ret.view->ssd, ret.node);
				if (ret.type == LAB_SSD_CLIENT) {
					ret.surface = lab_wlr_surface_from_node(ret.node);
				}
				return ret;
			case LAB_NODE_DESC_SSD_BUTTON: {
				/*
				 * Always return the top scene node for SSD
				 * buttons
				 */
				struct ssd_button *button =
					node_ssd_button_from_node(node);
				ret.node = node;
				ret.type = ssd_button_get_type(button);
				ret.view = ssd_button_get_view(button);
				return ret;
			}
			case LAB_NODE_DESC_LAYER_SURFACE:
				ret.node = node;
				ret.type = LAB_SSD_LAYER_SURFACE;
				ret.surface = get_surface_from_layer_node(node);
				return ret;
			case LAB_NODE_DESC_LAYER_POPUP:
				ret.node = node;
				ret.type = LAB_SSD_CLIENT;
				ret.surface = get_surface_from_layer_node(node);
				return ret;
			case LAB_NODE_DESC_MENUITEM:
				/* Always return the top scene node for menu items */
				ret.node = node;
				ret.type = LAB_SSD_MENU;
				return ret;
			case LAB_NODE_DESC_NODE:
			case LAB_NODE_DESC_TREE:
				break;
			}
		}

		/* Edge-case nodes without node-descriptors */
		if (node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_surface *surface = lab_wlr_surface_from_node(node);
			if (surface) {
				if (is_layer_descendant(node)) {
					/*
					 * layer-shell subsurfaces need to be
					 * able to receive pointer actions.
					 *
					 * Test by running
					 * `gtk-layer-demo -k exclusive`, then
					 * open the 'set margin' dialog and try
					 * setting the margin with the pointer.
					 */
					ret.surface = surface;
					return ret;
				}
			}
		}

		/* node->parent is always a *wlr_scene_tree */
		node = node->parent ? &node->parent->node : NULL;
	}
	wlr_log(WLR_ERROR, "Unknown node detected");
	return ret;
}
