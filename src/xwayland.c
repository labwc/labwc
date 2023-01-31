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
#include "workspaces.h"
#include "xwayland.h"

static int
round_to_increment(int val, int base, int inc)
{
	if (base < 0 || inc <= 0)
		return val;
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

static struct wlr_xwayland_surface *
top_parent_of(struct view *view)
{
	struct wlr_xwayland_surface *s = xwayland_surface_from_view(view);
	while (s->parent) {
		s = s->parent;
	}
	return s;
}

void
xwayland_move_sub_views_to_front(struct view *parent,
		void (*move_to_front)(struct view *view))
{
	assert(parent);
	assert(move_to_front);

	if (parent->type != LAB_XWAYLAND_VIEW) {
		return;
	}

	struct wlr_xwayland_surface *parent_xwayland_surface =
		xwayland_surface_from_view(parent);
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
		if (top_parent_of(view) != parent_xwayland_surface) {
			continue;
		}
		move_to_front(view);
		/* TODO: we should probably focus on these too here */
	}
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
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	assert(data && data == view->surface);

	/* Must receive commit signal before accessing surface->current* */
	struct wlr_surface_state *state = &view->surface->current;
	struct view_pending_move_resize *pending = &view->pending_move_resize;

	if (view->w == state->width && view->h == state->height) {
		return;
	}

	view->w = state->width;
	view->h = state->height;

	if (view->x != pending->x) {
		/* Adjust x for queued up configure events */
		view->x = pending->x + pending->width - view->w;
	}
	if (view->y != pending->y) {
		/* Adjust y for queued up configure events */
		view->y = pending->y + pending->height - view->h;
	}
	view_moved(view);
}

static void
handle_request_move(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
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
	 * the provied serial against a list of button press serials sent to
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
	view->impl->unmap(view);

	/*
	 * Some xwayland clients leave unmapped child views around, typically
	 * when a dialog window is closed. Although handle_destroy() is not
	 * called for these, we have to deal with them as such in terms of the
	 * foreign-toplevel protocol to avoid panels and the like still showing
	 * them.
	 */
	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_destroy(view->toplevel_handle);
		view->toplevel_handle = NULL;
	}
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
	assert(data && data == xwayland_view->xwayland_surface);
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

	/* Remove XWayland specific handlers */
	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_activate.link);
	wl_list_remove(&view->request_minimize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	wl_list_remove(&view->set_title.link);

	wl_list_remove(&xwayland_view->request_configure.link);
	wl_list_remove(&xwayland_view->set_app_id.link);
	wl_list_remove(&xwayland_view->set_decorations.link);
	wl_list_remove(&xwayland_view->override_redirect.link);

	wl_list_remove(&view->destroy.link);

	/* And finally destroy / free the view */
	view_destroy(view);
}

static void
configure(struct view *view, struct wlr_box geo)
{
	view->pending_move_resize.x = geo.x;
	view->pending_move_resize.y = geo.y;
	view->pending_move_resize.width = geo.width;
	view->pending_move_resize.height = geo.height;

	wlr_xwayland_surface_configure(xwayland_surface_from_view(view),
		geo.x, geo.y, geo.width, geo.height);

	/* If not resizing, process the move immediately */
	if (view->w == geo.width && view->h == geo.height) {
		view->x = geo.x;
		view->y = geo.y;
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

	configure(view, (struct wlr_box){event->x, event->y, width, height});
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_activate);
	assert(view);
	desktop_focus_and_activate_view(&view->server->seat, view);
	desktop_move_to_front(view);
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_minimize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_minimize);
	assert(view);
	view_minimize(view, event->minimize);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_maximize);
	assert(view);
	view_toggle_maximize(view);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_fullscreen);
	bool fullscreen = xwayland_surface_from_view(view)->fullscreen;
	view_set_fullscreen(view, fullscreen, NULL);
}

static void
handle_set_title(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, set_title);
	assert(view);
	view_update_title(view);
}

static void
handle_set_class(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_app_id);
	struct view *view = &xwayland_view->base;
	assert(view);
	view_update_app_id(view);
}

static void
move(struct view *view, int x, int y)
{
	view->x = x;
	view->y = y;

	/* override any previous pending move */
	view->pending_move_resize.x = x;
	view->pending_move_resize.y = y;

	struct wlr_xwayland_surface *s = xwayland_surface_from_view(view);
	wlr_xwayland_surface_configure(s, (int16_t)x, (int16_t)y,
		(uint16_t)s->width, (uint16_t)s->height);
	view_moved(view);
}

static void
_close(struct view *view)
{
	wlr_xwayland_surface_close(xwayland_surface_from_view(view));
}

static const char *
get_string_prop(struct view *view, const char *prop)
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

static bool
want_deco(struct wlr_xwayland_surface *xwayland_surface)
{
	return xwayland_surface->decorations ==
		WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
}

static void
handle_set_decorations(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_decorations);
	struct view *view = &xwayland_view->base;
	assert(data && data == xwayland_view->xwayland_surface);

	view_set_decorations(view, want_deco(xwayland_view->xwayland_surface));
}

static void
handle_override_redirect(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, override_redirect);
	struct view *view = &xwayland_view->base;
	struct wlr_xwayland_surface *xsurface = data;
	assert(xsurface && xsurface == xwayland_view->xwayland_surface);

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
		struct wlr_box box =
			output_usable_area_from_cursor_coords(view->server);
		view->x = box.x;
		view->y = box.y;
		view_center(view);
	}
}

static void
top_left_edge_boundary_check(struct view *view)
{
	struct wlr_box deco = ssd_max_extents(view);
	if (deco.x < 0) {
		view->x -= deco.x;
	}
	if (deco.y < 0) {
		view->y -= deco.y;
	}
	struct wlr_box box = {
		.x = view->x, .y = view->y, .width = view->w, .height = view->h
	};
	view->impl->configure(view, box);
}

static void
map(struct view *view)
{
	if (view->mapped) {
		return;
	}
	view->mapped = true;
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	struct wlr_xwayland_surface *xwayland_surface =
		xwayland_surface_from_view(view);
	if (!view->fullscreen && xwayland_surface->fullscreen) {
		view_set_fullscreen(view, true, NULL);
	}
	if (!view->maximized && !view->fullscreen) {
		view->x = xwayland_surface->x;
		view->y = xwayland_surface->y;
		view->w = xwayland_surface->width;
		view->h = xwayland_surface->height;
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

	if (!view->toplevel_handle) {
		foreign_toplevel_handle_create(view);
	}

	if (!view->been_mapped) {
		view_set_decorations(view, want_deco(xwayland_surface));

		if (!view->maximized && !view->fullscreen) {
			set_initial_position(view, xwayland_surface);
		}

		view_moved(view);
		view->been_mapped = true;
	}

	if (view->ssd_enabled && !view->fullscreen && !view->maximized) {
		top_left_edge_boundary_check(view);
	}

	/* Add commit here, as xwayland map/unmap can change the wlr_surface */
	wl_signal_add(&xwayland_surface->surface->events.commit, &view->commit);
	view->commit.notify = handle_commit;

	view_impl_map(view);
}

static void
unmap(struct view *view)
{
	if (!view->mapped) {
		return;
	}
	view->mapped = false;
	wl_list_remove(&view->commit.link);
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	desktop_focus_topmost_mapped_view(view->server);
}

static void
maximize(struct view *view, bool maximized)
{
	wlr_xwayland_surface_set_maximized(xwayland_surface_from_view(view),
		maximized);
}

static void
set_activated(struct view *view, bool activated)
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
set_fullscreen(struct view *view, bool fullscreen)
{
	wlr_xwayland_surface_set_fullscreen(xwayland_surface_from_view(view),
		fullscreen);
}

static const struct view_impl xwl_view_impl = {
	.configure = configure,
	.close = _close,
	.get_string_prop = get_string_prop,
	.map = map,
	.move = move,
	.set_activated = set_activated,
	.set_fullscreen = set_fullscreen,
	.unmap = unmap,
	.maximize = maximize
};

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
		return;
	}

	struct xwayland_view *xwayland_view = znew(*xwayland_view);
	struct view *view = &xwayland_view->base;

	view->server = server;
	view->type = LAB_XWAYLAND_VIEW;
	view->impl = &xwl_view_impl;

	/*
	 * Set two-way view <-> xsurface association.  Usually the
	 * association remains until the xsurface is destroyed (which
	 * also destroys the view).  The only exception is caused by
	 * setting override-redirect on the xsurface, which removes it
	 * from the view (destroying the view) and makes it an
	 * "unmanaged" surface.
	 */
	xwayland_view->xwayland_surface = xsurface;
	xsurface->data = view;

	view->workspace = server->workspace_current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	node_descriptor_create(&view->scene_tree->node,
		LAB_NODE_DESC_VIEW, view);

	view->map.notify = handle_map;
	wl_signal_add(&xsurface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xsurface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xsurface->events.destroy, &view->destroy);
	view->request_activate.notify = handle_request_activate;
	wl_signal_add(&xsurface->events.request_activate, &view->request_activate);
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
	xwayland_view->request_configure.notify = handle_request_configure;
	wl_signal_add(&xsurface->events.request_configure,
		&xwayland_view->request_configure);

	xwayland_view->set_app_id.notify = handle_set_class;
	wl_signal_add(&xsurface->events.set_class, &xwayland_view->set_app_id);

	xwayland_view->set_decorations.notify = handle_set_decorations;
	wl_signal_add(&xsurface->events.set_decorations,
		&xwayland_view->set_decorations);

	xwayland_view->override_redirect.notify = handle_override_redirect;
	wl_signal_add(&xsurface->events.set_override_redirect,
		&xwayland_view->override_redirect);

	wl_list_insert(&view->server->views, &view->link);
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
	wlr_xwayland_destroy(server->xwayland);
	server->xwayland = NULL;
}
