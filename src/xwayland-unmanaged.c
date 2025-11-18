// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/xwayland.h>
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "labwc.h"
#include "xwayland.h"

static void
handle_grab_focus(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, grab_focus);

	unmanaged->ever_grabbed_focus = true;
	if (unmanaged->node) {
		assert(unmanaged->xwayland_surface->surface);
		seat_focus_surface(&unmanaged->server->seat,
			unmanaged->xwayland_surface->surface);
	}
}

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

	if (wlr_xwayland_surface_override_redirect_wants_focus(xsurface)
			|| unmanaged->ever_grabbed_focus) {
		seat_focus_surface(&unmanaged->server->seat, xsurface->surface);
	}

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
		if (wlr_xwayland_surface_override_redirect_wants_focus(prev)
				|| u->ever_grabbed_focus) {
			seat_focus_surface(&server->seat, prev->surface);
			return;
		}
	}

	/*
	 * Unmanaged surfaces do not clear the active view when mapped.
	 * Therefore, we can simply give the focus back to the active
	 * view when the last unmanaged surface is unmapped.
	 *
	 * Also note that resetting the focus here is only on the
	 * compositor side. On the xwayland server side, focus is never
	 * given to unmanaged surfaces to begin with - keyboard grabs
	 * are used instead.
	 *
	 * In the case of Globally Active input windows, calling
	 * view_offer_focus() at this point is both unnecessary and
	 * insufficient, since it doesn't update the seat focus
	 * immediately and ultimately results in a loss of focus.
	 *
	 * For the above reasons, we avoid calling desktop_focus_view()
	 * here and instead call seat_focus_surface() directly.
	 *
	 * If modifying this logic, please test for regressions with
	 * menus/tooltips in JetBrains CLion or similar.
	 */
	if (server->active_view) {
		seat_focus_surface(&server->seat, server->active_view->surface);
	}
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

	/*
	 * Destroy the scene node. It would get destroyed later when
	 * the wlr_surface is destroyed, but if the unmanaged surface
	 * gets converted to a managed surface, that may be a while.
	 */
	wlr_scene_node_destroy(unmanaged->node);
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
	wl_list_remove(&unmanaged->grab_focus.link);
	wl_list_remove(&unmanaged->request_activate.link);
	wl_list_remove(&unmanaged->request_configure.link);
	wl_list_remove(&unmanaged->set_override_redirect.link);
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
	 * a child of the active view before granting the request.
	 *
	 * FIXME: this logic is a bit incomplete/inconsistent. Refer to
	 * https://github.com/labwc/labwc/discussions/2821 for more info.
	 */
	struct view *view = server->active_view;
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
	CONNECT_SIGNAL(xsurface, unmanaged, grab_focus);
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
