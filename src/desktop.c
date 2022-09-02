// SPDX-License-Identifier: GPL-2.0-only
#include "config.h"
#include <assert.h>
#include "labwc.h"
#include "layers.h"
#include "node.h"
#include "ssd.h"
#include "common/scene-helpers.h"
#include "workspaces.h"

static void
move_to_front(struct view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

#if HAVE_XWAYLAND
static struct wlr_xwayland_surface *
top_parent_of(struct view *view)
{
	struct wlr_xwayland_surface *s = view->xwayland_surface;
	while (s->parent) {
		s = s->parent;
	}
	return s;
}

static void
move_xwayland_sub_views_to_front(struct view *parent)
{
	if (!parent || parent->type != LAB_XWAYLAND_VIEW) {
		return;
	}
	struct view *view, *next;
	wl_list_for_each_reverse_safe(view, next, &parent->server->views, link)
	{
		/* need to stop here, otherwise loops keeps going forever */
		if (view == parent) {
			break;
		}
		if (view->type != LAB_XWAYLAND_VIEW) {
			continue;
		}
		if (!view->mapped && !view->minimized) {
			continue;
		}
		if (top_parent_of(view) != parent->xwayland_surface) {
			continue;
		}
		move_to_front(view);
		/* TODO: we should probably focus on these too here */
	}
}
#endif

void
desktop_move_to_front(struct view *view)
{
	if (!view) {
		return;
	}
	move_to_front(view);
#if HAVE_XWAYLAND
	move_xwayland_sub_views_to_front(view);
#endif
	cursor_update_focus(view->server);
}

static void
wl_list_insert_tail(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list->prev;
	elm->next = list;
	list->prev = elm;
	elm->prev->next = elm;
}

void
desktop_move_to_back(struct view *view)
{
	if (!view) {
		return;
	}
	wl_list_remove(&view->link);
	wl_list_insert_tail(&view->server->views, &view->link);
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
		return node_view_from_node(node);
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
	struct wl_list *(*iter)(struct wl_list *);

	/* Scene nodes are ordered like last node == displayed topmost */
	iter = dir == LAB_CYCLE_DIR_FORWARD ? get_prev_item : get_next_item;

	/* Make sure to have all nodes in their actual ordering */
	osd_preview_restore(server);

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
	wl_list_for_each (view, &server->views, link) {
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

struct view *
desktop_node_and_view_at(struct server *server, double lx, double ly,
		struct wlr_scene_node **scene_node, double *sx, double *sy,
		enum ssd_part_type *view_area)
{
	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);

	*scene_node = node;
	if (!node) {
		*view_area = LAB_SSD_ROOT;
		return NULL;
	}
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_surface *surface = lab_wlr_surface_from_node(node);
		if (surface && wlr_surface_is_layer_surface(surface)) {
			*view_area = LAB_SSD_LAYER_SURFACE;
			return NULL;
		}
#if HAVE_XWAYLAND
		if (node->parent == server->unmanaged_tree) {
			*view_area = LAB_SSD_UNMANAGED;
			return NULL;
		}
#endif
	}
	while (node) {
		struct node_descriptor *desc = node->data;
		/* TODO: convert to switch() */
		if (desc) {
			if (desc->type == LAB_NODE_DESC_VIEW) {
				goto has_view_data;
			}
			if (desc->type == LAB_NODE_DESC_XDG_POPUP) {
				goto has_view_data;
			}
			if (desc->type == LAB_NODE_DESC_SSD_BUTTON) {
				/* Always return the top scene node for SSD buttons */
				struct ssd_button *button = node_ssd_button_from_node(node);
				*scene_node = node;
				*view_area = button->type;
				return button->view;
			}
			if (desc->type == LAB_NODE_DESC_LAYER_SURFACE) {
				/* FIXME: we shouldn't have to set *view_area */
				*view_area = LAB_SSD_CLIENT;
				return NULL;
			}
			if (desc->type == LAB_NODE_DESC_LAYER_POPUP) {
				/* FIXME: we shouldn't have to set *view_area */
				*view_area = LAB_SSD_CLIENT;
				return NULL;
			}
			if (desc->type == LAB_NODE_DESC_MENUITEM) {
				/* Always return the top scene node for menu items */
				*scene_node = node;
				*view_area = LAB_SSD_MENU;
				return NULL;
			}
		}
		/* node->parent is always a *wlr_scene_tree */
		node = node->parent ? &node->parent->node : NULL;
	}
	if (!node) {
		wlr_log(WLR_ERROR, "Unknown node detected");
	}
	*view_area = LAB_SSD_NONE;
	return NULL;

struct view *view;
struct node_descriptor *desc;
has_view_data:
	desc = node->data;
	view = desc->data;
	*view_area = ssd_get_part_type(view, *scene_node);
	return view;
}

struct view *
desktop_view_at_cursor(struct server *server)
{
	double sx, sy;
	struct wlr_scene_node *node;
	enum ssd_part_type view_area = LAB_SSD_NONE;

	return desktop_node_and_view_at(server,
			server->seat.cursor->x, server->seat.cursor->y,
			&node, &sx, &sy, &view_area);
}
