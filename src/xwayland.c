// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <wlr/xwayland.h>
#include "common/mem.h"
#include "labwc.h"
#include "node.h"
#include "ssd.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"
#include "workspaces.h"
#include "xwayland.h"

static int
round_to_increment(int val, int base, int inc)
{
	if (base < 0 || inc <= 0) {
		return val;
	}
	return base + (val - base + inc / 2) / inc * inc;
}

bool
xwayland_apply_size_hints(struct view *view, int *w, int *h)
{
	assert(view);
	if (view->type == LAB_XWAYLAND_VIEW) {
		xcb_size_hints_t *hints =
			xwayland_surface_from_view(view)->size_hints;

		/*
		 * Honor size increments from WM_SIZE_HINTS. Typically, X11
		 * terminal emulators will use WM_SIZE_HINTS to make sure that
		 * the terminal is resized to a width/height evenly divisible by
		 * the cell (character) size.
		 */
		if (hints) {
			*w = round_to_increment(*w, hints->base_width,
				hints->width_inc);
			*h = round_to_increment(*h, hints->base_height,
				hints->height_inc);

			*w = MAX(*w, MAX(1, hints->min_width));
			*h = MAX(*h, MAX(1, hints->min_height));
			return true;
		}
	}
	return false;
}

static void
xwayland_view_fill_size_hints(struct view *view, struct wlr_box *box)
{
	if (view->type == LAB_XWAYLAND_VIEW) {
		xcb_size_hints_t *hints = xwayland_surface_from_view(view)->size_hints;
		if (hints) {
			box->width = hints->width_inc;
			box->height = hints->height_inc;
			return;
		}
	}
	box->width = 0;
	box->height = 0;
}

static struct wlr_xwayland_surface *
top_parent_of(struct view *view)
{
	struct wlr_xwayland_surface *s = xwayland_surface_from_view(view);
	while (s->parent) {
		s = s->parent;
	}
	return s;
}

static struct xwayland_view *
xwayland_view_from_view(struct view *view)
{
	assert(view->type == LAB_XWAYLAND_VIEW);
	return (struct xwayland_view *)view;
}

struct wlr_xwayland_surface *
xwayland_surface_from_view(struct view *view)
{
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	assert(xwayland_view->xwayland_surface);
	return xwayland_view->xwayland_surface;
}

static void
ensure_initial_geometry_and_output(struct view *view)
{
	if (wlr_box_empty(&view->current)) {
		struct wlr_xwayland_surface *xwayland_surface =
			xwayland_surface_from_view(view);
		view->current.x = xwayland_surface->x;
		view->current.y = xwayland_surface->y;
		view->current.width = xwayland_surface->width;
		view->current.height = xwayland_surface->height;
		/*
		 * If there is no pending move/resize yet, then set
		 * current values (used in map()).
		 */
		if (wlr_box_empty(&view->pending)) {
			view->pending = view->current;
		}
	}
	if (!view->output) {
		/*
		 * Just use the cursor output since we don't know yet
		 * whether the surface position is meaningful.
		 */
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
}

static bool
want_deco(struct wlr_xwayland_surface *xwayland_surface)
{
	struct view *view = (struct view *)xwayland_surface->data;

	/* Window-rules take priority if they exist for this view */
	switch (window_rules_get_property(view, "serverDecoration")) {
	case LAB_PROP_TRUE:
		return true;
	case LAB_PROP_FALSE:
		return false;
	default:
		break;
	}

	return xwayland_surface->decorations ==
		WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	assert(data && data == view->surface);

	/* Must receive commit signal before accessing surface->current* */
	struct wlr_surface_state *state = &view->surface->current;
	struct wlr_box *current = &view->current;

	/*
	 * If there is a pending move/resize, wait until the surface
	 * size changes to update geometry. The hope is to update both
	 * the position and the size of the view at the same time,
	 * reducing visual glitches.
	 */
	if (current->width != state->width || current->height != state->height) {
		view_impl_apply_geometry(view, state->width, state->height);
	}
}

static void
handle_request_move(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provided serial against a list of button press serials sent to
	 * this client, to prevent the client from requesting this whenever they
	 * want.
	 */
	struct view *view = wl_container_of(listener, view, request_move);
	interactive_begin(view, LAB_INPUT_STATE_MOVE, 0);
}

static void
handle_request_resize(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provided serial against a list of button press serials sent to
	 * this client, to prevent the client from requesting this whenever they
	 * want.
	 */
	struct wlr_xwayland_resize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_resize);
	interactive_begin(view, LAB_INPUT_STATE_RESIZE, event->edges);
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, map);
	view->impl->map(view);
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, unmap);
	view->impl->unmap(view, /* client_request */ true);
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, surface_destroy);
	assert(data && data == view->surface);

	view->surface = NULL;
	wl_list_remove(&view->surface_destroy.link);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	assert(xwayland_view->xwayland_surface->data == view);

	if (view->surface) {
		/*
		 * We got the destroy signal from
		 * wlr_xwayland_surface before the
		 * destroy signal from wlr_surface.
		 */
		wl_list_remove(&view->surface_destroy.link);
	}
	view->surface = NULL;

	/*
	 * Break view <-> xsurface association.  Note that the xsurface
	 * may not actually be destroyed at this point; it may become an
	 * "unmanaged" surface instead.
	 */
	xwayland_view->xwayland_surface->data = NULL;
	xwayland_view->xwayland_surface = NULL;

	/* Remove XWayland view specific listeners */
	wl_list_remove(&xwayland_view->request_activate.link);
	wl_list_remove(&xwayland_view->request_configure.link);
	wl_list_remove(&xwayland_view->set_app_id.link);
	wl_list_remove(&xwayland_view->set_decorations.link);
	wl_list_remove(&xwayland_view->override_redirect.link);

	view_destroy(view);
}

static void
xwayland_view_configure(struct view *view, struct wlr_box geo)
{
	view->pending = geo;
	wlr_xwayland_surface_configure(xwayland_surface_from_view(view),
		geo.x, geo.y, geo.width, geo.height);

	/* If not resizing, process the move immediately */
	if (view->current.width == geo.width
			&& view->current.height == geo.height) {
		view->current.x = geo.x;
		view->current.y = geo.y;
		view_moved(view);
	}
}

static void
handle_request_configure(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_configure);
	struct view *view = &xwayland_view->base;
	struct wlr_xwayland_surface_configure_event *event = data;

	int width = event->width;
	int height = event->height;
	view_adjust_size(view, &width, &height);

	xwayland_view_configure(view,
		(struct wlr_box){event->x, event->y, width, height});
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_activate);
	struct view *view = &xwayland_view->base;

	if (window_rules_get_property(view, "ignoreFocusRequest") == LAB_PROP_TRUE) {
		wlr_log(WLR_INFO, "Ignoring focus request due to window rule configuration");
		return;
	}

	desktop_focus_and_activate_view(&view->server->seat, view);
	view_move_to_front(view);
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_minimize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_minimize);
	view_minimize(view, event->minimize);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_maximize);
	if (!view->mapped) {
		ensure_initial_geometry_and_output(view);
		/*
		 * Set decorations early to avoid changing geometry
		 * after maximize (reduces visual glitches).
		 */
		view_set_decorations(view,
			want_deco(xwayland_surface_from_view(view)));
	}
	view_toggle_maximize(view);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_fullscreen);
	bool fullscreen = xwayland_surface_from_view(view)->fullscreen;
	if (!view->mapped) {
		ensure_initial_geometry_and_output(view);
	}
	view_set_fullscreen(view, fullscreen);
}

static void
handle_set_title(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, set_title);
	view_update_title(view);
}

static void
handle_set_class(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_app_id);
	struct view *view = &xwayland_view->base;
	view_update_app_id(view);
}

static void
xwayland_view_close(struct view *view)
{
	wlr_xwayland_surface_close(xwayland_surface_from_view(view));
}

static const char *
xwayland_view_get_string_prop(struct view *view, const char *prop)
{
	struct wlr_xwayland_surface *xwayland_surface =
		xwayland_surface_from_view(view);
	if (!strcmp(prop, "title")) {
		return xwayland_surface->title;
	}
	if (!strcmp(prop, "class")) {
		return xwayland_surface->class;
	}
	/* We give 'class' for wlr_foreign_toplevel_handle_v1_set_app_id() */
	if (!strcmp(prop, "app_id")) {
		return xwayland_surface->class;
	}
	return "";
}

static void
handle_set_decorations(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_decorations);
	struct view *view = &xwayland_view->base;

	view_set_decorations(view, want_deco(xwayland_view->xwayland_surface));
}

static void
handle_override_redirect(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, override_redirect);
	struct view *view = &xwayland_view->base;
	struct wlr_xwayland_surface *xsurface = xwayland_view->xwayland_surface;

	struct server *server = view->server;
	bool mapped = xsurface->mapped;
	if (mapped) {
		handle_unmap(&view->unmap, xsurface);
	}
	handle_destroy(&view->destroy, xsurface);
	/* view is invalid after this point */
	xwayland_unmanaged_create(server, xsurface, mapped);
}

static void
set_initial_position(struct view *view,
		struct wlr_xwayland_surface *xwayland_surface)
{
	/* Don't center views with position explicitly specified */
	bool has_position = xwayland_surface->size_hints &&
		(xwayland_surface->size_hints->flags & (
			XCB_ICCCM_SIZE_HINT_US_POSITION |
			XCB_ICCCM_SIZE_HINT_P_POSITION));

	if (has_position) {
		/* Just make sure the view is on-screen */
		view_adjust_for_layout_change(view);
	} else {
		view_center(view, NULL);
	}
}

static void
top_left_edge_boundary_check(struct view *view)
{
	struct wlr_box deco = ssd_max_extents(view);
	if (deco.x < 0) {
		view->current.x -= deco.x;
	}
	if (deco.y < 0) {
		view->current.y -= deco.y;
	}
	view->impl->configure(view, view->current);
}

static void
init_foreign_toplevel(struct view *view)
{
	foreign_toplevel_handle_create(view);

	struct wlr_xwayland_surface *surface = xwayland_surface_from_view(view);
	if (!surface->parent) {
		return;
	}
	struct view *parent = (struct view *)surface->parent->data;
	if (!parent->toplevel.handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_parent(view->toplevel.handle, parent->toplevel.handle);
}

static void
xwayland_view_map(struct view *view)
{
	struct wlr_xwayland_surface *xwayland_surface = xwayland_surface_from_view(view);
	if (view->mapped) {
		return;
	}
	if (!xwayland_surface->surface) {
		/*
		 * We may get here if a user minimizes an xwayland dialog at the
		 * same time as the client requests unmap, which xwayland
		 * clients sometimes do without actually requesting destroy
		 * even if they don't intend to use that view/surface anymore
		 */
		wlr_log(WLR_DEBUG, "Cannot map view without wlr_surface");
		return;
	}
	view->mapped = true;
	ensure_initial_geometry_and_output(view);
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	if (!view->fullscreen && xwayland_surface->fullscreen) {
		view_set_fullscreen(view, true);
	}

	if (view->surface != xwayland_surface->surface) {
		if (view->surface) {
			wl_list_remove(&view->surface_destroy.link);
		}
		view->surface = xwayland_surface->surface;

		/* Required to set the surface to NULL when destroyed by the client */
		view->surface_destroy.notify = handle_surface_destroy;
		wl_signal_add(&view->surface->events.destroy, &view->surface_destroy);

		/* Will be free'd automatically once the surface is being destroyed */
		struct wlr_scene_tree *tree = wlr_scene_subsurface_tree_create(
			view->scene_tree, view->surface);
		if (!tree) {
			/* TODO: might need further clean up */
			wl_resource_post_no_memory(view->surface->resource);
			return;
		}
		view->scene_node = &tree->node;
	}

	if (!view->toplevel.handle) {
		init_foreign_toplevel(view);
	}

	if (!view->been_mapped) {
		view_set_decorations(view, want_deco(xwayland_surface));

		if (view_is_floating(view)) {
			set_initial_position(view, xwayland_surface);
		}

		/*
		 * When mapping the view for the first time, visual
		 * artifacts are reduced if we display it immediately at
		 * the final intended position/size rather than waiting
		 * for handle_commit().
		 */
		view->current = view->pending;
		view_moved(view);
	}

	if (view->ssd_enabled && view_is_floating(view)) {
		top_left_edge_boundary_check(view);
	}

	/* Add commit here, as xwayland map/unmap can change the wlr_surface */
	wl_signal_add(&xwayland_surface->surface->events.commit, &view->commit);
	view->commit.notify = handle_commit;

	view_impl_map(view);
	view->been_mapped = true;
}

static void
xwayland_view_unmap(struct view *view, bool client_request)
{
	if (!view->mapped) {
		goto out;
	}
	view->mapped = false;
	wl_list_remove(&view->commit.link);
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	desktop_focus_topmost_mapped_view(view->server);

	/*
	 * If the view was explicitly unmapped by the client (rather
	 * than just minimized), destroy the foreign toplevel handle so
	 * the unmapped view doesn't show up in panels and the like.
	 */
out:
	if (client_request && view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_destroy(view->toplevel.handle);
		view->toplevel.handle = NULL;
	}
}

static void
xwayland_view_maximize(struct view *view, bool maximized)
{
	wlr_xwayland_surface_set_maximized(xwayland_surface_from_view(view),
		maximized);
}

static void
xwayland_view_minimize(struct view *view, bool minimized)
{
	wlr_xwayland_surface_set_minimized(xwayland_surface_from_view(view),
		minimized);
}

static struct view *
xwayland_view_get_root(struct view *view)
{
	struct wlr_xwayland_surface *root = top_parent_of(view);
	return (struct view *)root->data;
}

static void
xwayland_view_move_to_front(struct view *view)
{
	struct view *root = xwayland_view_get_root(view);
	view_impl_move_to_front(root);
	view_impl_move_sub_views(root, LAB_TO_FRONT);
}

static void
xwayland_view_move_to_back(struct view *view)
{
	struct view *root = xwayland_view_get_root(view);
	view_impl_move_sub_views(root, LAB_TO_BACK);
	view_impl_move_to_back(root);
}

static void
xwayland_view_append_children(struct view *self, struct wl_array *children)
{
	struct wlr_xwayland_surface *surface = xwayland_surface_from_view(self);
	struct view *view;

	wl_list_for_each_reverse(view, &self->server->views, link)
	{
		if (view == self) {
			continue;
		}
		if (view->type != LAB_XWAYLAND_VIEW) {
			continue;
		}
		/*
		 * This happens when a view has never been mapped or when a
		 * client has requested a `handle_unmap`.
		 */
		if (!view->surface) {
			continue;
		}
		if (!view->mapped && !view->minimized) {
			continue;
		}
		if (top_parent_of(view) != surface) {
			continue;
		}
		struct view **child = wl_array_add(children, sizeof(*child));
		*child = view;
	}
}

static void
xwayland_view_set_activated(struct view *view, bool activated)
{
	struct wlr_xwayland_surface *xwayland_surface =
		xwayland_surface_from_view(view);

	if (activated && xwayland_surface->minimized) {
		wlr_xwayland_surface_set_minimized(xwayland_surface, false);
	}

	wlr_xwayland_surface_activate(xwayland_surface, activated);
	if (activated) {
		wlr_xwayland_surface_restack(xwayland_surface,
			NULL, XCB_STACK_MODE_ABOVE);
		/* Restack unmanaged surfaces on top */
		struct xwayland_unmanaged *u;
		struct wl_list *list = &view->server->unmanaged_surfaces;
		wl_list_for_each(u, list, link) {
			wlr_xwayland_surface_restack(u->xwayland_surface,
				NULL, XCB_STACK_MODE_ABOVE);
		}
	}
}

static void
xwayland_view_set_fullscreen(struct view *view, bool fullscreen)
{
	wlr_xwayland_surface_set_fullscreen(xwayland_surface_from_view(view),
		fullscreen);
}

static const struct view_impl xwayland_view_impl = {
	.configure = xwayland_view_configure,
	.close = xwayland_view_close,
	.get_string_prop = xwayland_view_get_string_prop,
	.map = xwayland_view_map,
	.set_activated = xwayland_view_set_activated,
	.set_fullscreen = xwayland_view_set_fullscreen,
	.unmap = xwayland_view_unmap,
	.maximize = xwayland_view_maximize,
	.minimize = xwayland_view_minimize,
	.move_to_front = xwayland_view_move_to_front,
	.move_to_back = xwayland_view_move_to_back,
	.get_root = xwayland_view_get_root,
	.append_children = xwayland_view_append_children,
	.fill_size_hints = xwayland_view_fill_size_hints,
};

void
xwayland_view_create(struct server *server,
		struct wlr_xwayland_surface *xsurface, bool mapped)
{
	struct xwayland_view *xwayland_view = znew(*xwayland_view);
	struct view *view = &xwayland_view->base;

	view->server = server;
	view->type = LAB_XWAYLAND_VIEW;
	view->impl = &xwayland_view_impl;

	/*
	 * Set two-way view <-> xsurface association.  Usually the association
	 * remains until the xsurface is destroyed (which also destroys the
	 * view).  The only exception is caused by setting override-redirect on
	 * the xsurface, which removes it from the view (destroying the view)
	 * and makes it an "unmanaged" surface.
	 */
	xwayland_view->xwayland_surface = xsurface;
	xsurface->data = view;

	view->workspace = server->workspace_current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	node_descriptor_create(&view->scene_tree->node, LAB_NODE_DESC_VIEW, view);

	view->map.notify = handle_map;
	wl_signal_add(&xsurface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xsurface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xsurface->events.destroy, &view->destroy);
	view->request_minimize.notify = handle_request_minimize;
	wl_signal_add(&xsurface->events.request_minimize, &view->request_minimize);
	view->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&xsurface->events.request_maximize, &view->request_maximize);
	view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&xsurface->events.request_fullscreen, &view->request_fullscreen);
	view->request_move.notify = handle_request_move;
	wl_signal_add(&xsurface->events.request_move, &view->request_move);
	view->request_resize.notify = handle_request_resize;
	wl_signal_add(&xsurface->events.request_resize, &view->request_resize);

	view->set_title.notify = handle_set_title;
	wl_signal_add(&xsurface->events.set_title, &view->set_title);

	/* Events specific to XWayland views */
	xwayland_view->request_activate.notify = handle_request_activate;
	wl_signal_add(&xsurface->events.request_activate, &xwayland_view->request_activate);

	xwayland_view->request_configure.notify = handle_request_configure;
	wl_signal_add(&xsurface->events.request_configure, &xwayland_view->request_configure);

	xwayland_view->set_app_id.notify = handle_set_class;
	wl_signal_add(&xsurface->events.set_class, &xwayland_view->set_app_id);

	xwayland_view->set_decorations.notify = handle_set_decorations;
	wl_signal_add(&xsurface->events.set_decorations, &xwayland_view->set_decorations);

	xwayland_view->override_redirect.notify = handle_override_redirect;
	wl_signal_add(&xsurface->events.set_override_redirect, &xwayland_view->override_redirect);

	wl_list_insert(&view->server->views, &view->link);

	if (mapped) {
		xwayland_view_map(view);
	}
}

static void
handle_new_surface(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, xwayland_new_surface);
	struct wlr_xwayland_surface *xsurface = data;
	wlr_xwayland_surface_ping(xsurface);

	/*
	 * We do not create 'views' for xwayland override_redirect surfaces,
	 * but add them to server.unmanaged_surfaces so that we can render them
	 */
	if (xsurface->override_redirect) {
		xwayland_unmanaged_create(server, xsurface, /* mapped */ false);
	} else {
		xwayland_view_create(server, xsurface, /* mapped */ false);
	}
}

static void
handle_ready(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, xwayland_ready);
	wlr_xwayland_set_seat(server->xwayland, server->seat.seat);
}

void
xwayland_server_init(struct server *server, struct wlr_compositor *compositor)
{
	server->xwayland =
		wlr_xwayland_create(server->wl_display, compositor, true);
	if (!server->xwayland) {
		wlr_log(WLR_ERROR, "cannot create xwayland server");
		exit(EXIT_FAILURE);
	}
	server->xwayland_new_surface.notify = handle_new_surface;
	wl_signal_add(&server->xwayland->events.new_surface,
		&server->xwayland_new_surface);

	server->xwayland_ready.notify = handle_ready;
	wl_signal_add(&server->xwayland->events.ready,
		&server->xwayland_ready);

	if (setenv("DISPLAY", server->xwayland->display_name, true) < 0) {
		wlr_log_errno(WLR_ERROR, "unable to set DISPLAY for xwayland");
	} else {
		wlr_log(WLR_DEBUG, "xwayland is running on display %s",
			server->xwayland->display_name);
	}

	struct wlr_xcursor *xcursor;
	xcursor = wlr_xcursor_manager_get_xcursor(
		server->seat.xcursor_manager, XCURSOR_DEFAULT, 1);
	if (xcursor) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server->xwayland, image->buffer,
			image->width * 4, image->width,
			image->height, image->hotspot_x,
			image->hotspot_y);
	}
}

void
xwayland_server_finish(struct server *server)
{
	struct wlr_xwayland *xwayland = server->xwayland;
	/*
	 * Reset server->xwayland to NULL first to prevent callbacks (like
	 * server_global_filter) from accessing it as it is destroyed
	 */
	server->xwayland = NULL;
	wlr_xwayland_destroy(xwayland);
}
