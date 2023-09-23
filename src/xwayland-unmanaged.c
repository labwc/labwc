// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/xwayland.h>
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "xwayland.h"

static void
unmanaged_handle_request_configure(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, request_configure);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	struct wlr_xwayland_surface_configure_event *ev = data;
	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width, ev->height);
	if (unmanaged->node) {
		wlr_scene_node_set_position(unmanaged->node, ev->x, ev->y);
		cursor_update_focus(unmanaged->server);
	}
}

static void
unmanaged_handle_set_geometry(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, set_geometry);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	if (unmanaged->node) {
		wlr_scene_node_set_position(unmanaged->node, xsurface->x, xsurface->y);
		cursor_update_focus(unmanaged->server);
	}
}

static void
unmanaged_handle_map(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, map);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	assert(!unmanaged->node);

	/* Stack new surface on top */
	wlr_xwayland_surface_restack(xsurface, NULL, XCB_STACK_MODE_ABOVE);
	wl_list_append(&unmanaged->server->unmanaged_surfaces, &unmanaged->link);

	wl_signal_add(&xsurface->events.set_geometry, &unmanaged->set_geometry);
	unmanaged->set_geometry.notify = unmanaged_handle_set_geometry;

	if (wlr_xwayland_or_surface_wants_focus(xsurface)) {
		seat_focus_surface(&unmanaged->server->seat, xsurface->surface);
	}

	/* node will be destroyed automatically once surface is destroyed */
	unmanaged->node = &wlr_scene_surface_create(
			unmanaged->server->unmanaged_tree,
			xsurface->surface)->buffer->node;
	wlr_scene_node_set_position(unmanaged->node, xsurface->x, xsurface->y);
	cursor_update_focus(unmanaged->server);
}

static void
focus_next_surface(struct server *server, struct wlr_xwayland_surface *xsurface)
{
	/*
	 * Try to focus on parent surface
	 * This seems to fix JetBrains/Intellij window focus issues
	 */
	if (xsurface->parent && xsurface->parent->surface
			&& wlr_xwayland_or_surface_wants_focus(xsurface->parent)) {
		seat_focus_surface(&server->seat, xsurface->parent->surface);
		return;
	}

	/* Try to focus on last created unmanaged xwayland surface */
	struct xwayland_unmanaged *u;
	struct wl_list *list = &server->unmanaged_surfaces;
	wl_list_for_each_reverse(u, list, link) {
		struct wlr_xwayland_surface *prev = u->xwayland_surface;
		if (wlr_xwayland_or_surface_wants_focus(prev)) {
			seat_focus_surface(&server->seat, prev->surface);
			return;
		}
	}

	/*
	 * If we don't find a surface to focus fall back
	 * to the topmost mapped view. This fixes dmenu
	 * not giving focus back when closed with ESC.
	 */
	desktop_focus_topmost_view(server);
}

static void
unmanaged_handle_unmap(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, unmap);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	struct seat *seat = &unmanaged->server->seat;
	assert(unmanaged->node);

	wl_list_remove(&unmanaged->link);
	wl_list_remove(&unmanaged->set_geometry.link);
	wlr_scene_node_set_enabled(unmanaged->node, false);

	/*
	 * Mark the node as gone so a racing configure event
	 * won't try to reposition the node while unmapped.
	 */
	unmanaged->node = NULL;
	cursor_update_focus(unmanaged->server);

	if (seat->seat->keyboard_state.focused_surface == xsurface->surface) {
		focus_next_surface(unmanaged->server, xsurface);
	}
}

static void
unmanaged_handle_destroy(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, destroy);
	wl_list_remove(&unmanaged->request_configure.link);
	wl_list_remove(&unmanaged->override_redirect.link);
	wl_list_remove(&unmanaged->request_activate.link);
	wl_list_remove(&unmanaged->map.link);
	wl_list_remove(&unmanaged->unmap.link);
	wl_list_remove(&unmanaged->destroy.link);
	free(unmanaged);
}

static void
unmanaged_handle_override_redirect(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "handle unmanaged override_redirect");
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, override_redirect);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	struct server *server = unmanaged->server;

	bool mapped = xsurface->mapped;
	if (mapped) {
		unmanaged_handle_unmap(&unmanaged->unmap, NULL);
	}
	unmanaged_handle_destroy(&unmanaged->destroy, NULL);
	xsurface->data = NULL;

	xwayland_view_create(server, xsurface, mapped);
}

static void
unmanaged_handle_request_activate(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "handle unmanaged request_activate");
	struct wlr_xwayland_surface *xsurface = data;
	if (!xsurface->mapped) {
		return;
	}
	struct xwayland_unmanaged *unmanaged = xsurface->data;
	struct server *server = unmanaged->server;
	struct seat *seat = &server->seat;

	/*
	 * Validate that the unmanaged surface trying to grab focus is actually
	 * a child of the topmost mapped view before granting the request.
	 */
	struct view *view = desktop_topmost_focusable_view(server);
	if (view && view->type == LAB_XWAYLAND_VIEW) {
		struct wlr_xwayland_surface *surf =
			wlr_xwayland_surface_from_wlr_surface(view->surface);
		if (surf && surf->pid != xsurface->pid) {
			return;
		}
	}

	seat_focus_surface(seat, xsurface->surface);
}

void
xwayland_unmanaged_create(struct server *server,
		struct wlr_xwayland_surface *xsurface, bool mapped)
{
	struct xwayland_unmanaged *unmanaged = znew(*unmanaged);
	unmanaged->server = server;
	unmanaged->xwayland_surface = xsurface;
	xsurface->data = unmanaged;

	wl_signal_add(&xsurface->events.request_configure,
		&unmanaged->request_configure);
	unmanaged->request_configure.notify =
		unmanaged_handle_request_configure;

	wl_signal_add(&xsurface->events.map, &unmanaged->map);
	unmanaged->map.notify = unmanaged_handle_map;

	wl_signal_add(&xsurface->events.unmap, &unmanaged->unmap);
	unmanaged->unmap.notify = unmanaged_handle_unmap;

	wl_signal_add(&xsurface->events.destroy, &unmanaged->destroy);
	unmanaged->destroy.notify = unmanaged_handle_destroy;

	wl_signal_add(&xsurface->events.set_override_redirect,
		&unmanaged->override_redirect);
	unmanaged->override_redirect.notify = unmanaged_handle_override_redirect;

	wl_signal_add(&xsurface->events.request_activate,
		&unmanaged->request_activate);
	unmanaged->request_activate.notify = unmanaged_handle_request_activate;

	if (mapped) {
		unmanaged_handle_map(&unmanaged->map, xsurface);
	}
}
