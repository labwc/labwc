// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/xwayland.h>
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "labwc.h"
#include "xwayland.h"

static void
handle_request_configure(struct wl_listener *listener, void *data)
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
handle_set_geometry(struct wl_listener *listener, void *data)
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
handle_map(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, mappable.map);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	assert(!unmanaged->node);

	/* Stack new surface on top */
	wl_list_append(&unmanaged->server->unmanaged_surfaces, &unmanaged->link);

	CONNECT_SIGNAL(xsurface, unmanaged, set_geometry);

	if (wlr_xwayland_surface_override_redirect_wants_focus(xsurface)) {
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
	/* Try to focus on last created unmanaged xwayland surface */
	struct xwayland_unmanaged *u;
	struct wl_list *list = &server->unmanaged_surfaces;
	wl_list_for_each_reverse(u, list, link) {
		struct wlr_xwayland_surface *prev = u->xwayland_surface;
		if (wlr_xwayland_surface_override_redirect_wants_focus(prev)) {
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
handle_unmap(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, mappable.unmap);
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
handle_associate(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, associate);
	assert(unmanaged->xwayland_surface &&
		unmanaged->xwayland_surface->surface);

	mappable_connect(&unmanaged->mappable,
		unmanaged->xwayland_surface->surface,
		handle_map, handle_unmap);
}

static void
handle_dissociate(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, dissociate);

	if (!unmanaged->mappable.connected) {
		/*
		 * In some cases wlroots fails to emit the associate event
		 * due to an early return in xwayland_surface_associate().
		 * This is arguably a wlroots bug, but nevertheless it
		 * should not bring down labwc.
		 *
		 * TODO: Potentially remove when starting to track
		 *       wlroots 0.18 and it got fixed upstream.
		 */
		wlr_log(WLR_ERROR, "dissociate received before associate");
		return;
	}
	mappable_disconnect(&unmanaged->mappable);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, destroy);

	if (unmanaged->mappable.connected) {
		mappable_disconnect(&unmanaged->mappable);
	}

	wl_list_remove(&unmanaged->associate.link);
	wl_list_remove(&unmanaged->dissociate.link);
	wl_list_remove(&unmanaged->request_configure.link);
	wl_list_remove(&unmanaged->set_override_redirect.link);
	wl_list_remove(&unmanaged->request_activate.link);
	wl_list_remove(&unmanaged->destroy.link);
	free(unmanaged);
}

static void
handle_set_override_redirect(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "handle unmanaged override_redirect");
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, set_override_redirect);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	struct server *server = unmanaged->server;

	bool mapped = xsurface->surface && xsurface->surface->mapped;
	if (mapped) {
		handle_unmap(&unmanaged->mappable.unmap, NULL);
	}
	handle_destroy(&unmanaged->destroy, NULL);

	xwayland_view_create(server, xsurface, mapped);
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "handle unmanaged request_activate");
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, request_activate);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	if (!xsurface->surface || !xsurface->surface->mapped) {
		return;
	}
	struct server *server = unmanaged->server;
	struct seat *seat = &server->seat;

	/*
	 * Validate that the unmanaged surface trying to grab focus is actually
	 * a child of the topmost mapped view before granting the request.
	 */
	struct view *view = desktop_topmost_focusable_view(server);
	if (view && view->type == LAB_XWAYLAND_VIEW) {
		struct wlr_xwayland_surface *surf =
			wlr_xwayland_surface_try_from_wlr_surface(view->surface);
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
	/*
	 * xsurface->data is presumed to be a (struct view *) if set,
	 * so it must be left NULL for an unmanaged surface (it should
	 * be NULL already at this point).
	 */
	assert(!xsurface->data);

	CONNECT_SIGNAL(xsurface, unmanaged, associate);
	CONNECT_SIGNAL(xsurface, unmanaged, dissociate);
	CONNECT_SIGNAL(xsurface, unmanaged, destroy);
	CONNECT_SIGNAL(xsurface, unmanaged, request_activate);
	CONNECT_SIGNAL(xsurface, unmanaged, request_configure);
	CONNECT_SIGNAL(xsurface, unmanaged, set_override_redirect);

	if (xsurface->surface) {
		handle_associate(&unmanaged->associate, NULL);
	}
	if (mapped) {
		handle_map(&unmanaged->mappable.map, NULL);
	}
}
