// SPDX-License-Identifier: GPL-2.0-only
#include "labwc.h"

static void
unmanaged_handle_request_configure(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, request_configure);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	struct wlr_xwayland_surface_configure_event *ev = data;
	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width,
				       ev->height);
}

static void
unmanaged_handle_commit(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, commit);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	unmanaged->lx = xsurface->x;
	unmanaged->ly = xsurface->y;
}

static void
unmanaged_handle_set_geometry(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, set_geometry);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;

	if (xsurface->x != unmanaged->lx || xsurface->y != unmanaged->ly) {
		wlr_log(WLR_DEBUG, "xwayland-unmanaged surface has moved");
		unmanaged->lx = xsurface->x;
		unmanaged->ly = xsurface->y;
	}
}

void
unmanaged_handle_map(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, map);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;

	wl_list_insert(unmanaged->server->unmanaged_surfaces.prev,
		       &unmanaged->link);

	wl_signal_add(&xsurface->surface->events.commit, &unmanaged->commit);
	unmanaged->commit.notify = unmanaged_handle_commit;

	wl_signal_add(&xsurface->events.set_geometry, &unmanaged->set_geometry);
	unmanaged->set_geometry.notify = unmanaged_handle_set_geometry;

	unmanaged->lx = xsurface->x;
	unmanaged->ly = xsurface->y;
	if (wlr_xwayland_or_surface_wants_focus(xsurface)) {
		seat_focus_surface(&unmanaged->server->seat, xsurface->surface);
	}

	/* node will be destroyed automatically once surface is destroyed */
	struct wlr_scene_node *node = &wlr_scene_surface_create(
			&unmanaged->server->unmanaged_tree->node,
			xsurface->surface)->buffer->node;
	wlr_scene_node_set_position(node, unmanaged->lx, unmanaged->ly);
}

static void
unmanaged_handle_unmap(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, unmap);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	wl_list_remove(&unmanaged->link);
	wl_list_remove(&unmanaged->set_geometry.link);
	wl_list_remove(&unmanaged->commit.link);

	struct seat *seat = &unmanaged->server->seat;
	if (seat->seat->keyboard_state.focused_surface == xsurface->surface) {
		/*
		 * Try to focus on parent surface
		 * This seems to fix JetBrains/Intellij window focus issues
		 */
		if (xsurface->parent && xsurface->parent->surface
				&& wlr_xwayland_or_surface_wants_focus(xsurface->parent)) {
			seat_focus_surface(seat, xsurface->parent->surface);
			return;
		}

		/* Try to focus on last created unmanaged xwayland surface */
		struct xwayland_unmanaged *u;
		struct wl_list *list = &unmanaged->server->unmanaged_surfaces;
		wl_list_for_each (u, list, link) {
			struct wlr_xwayland_surface *prev = u->xwayland_surface;
			if (!wlr_xwayland_or_surface_wants_focus(prev)) {
				continue;
			}
			seat_focus_surface(seat, prev->surface);
			return;
		}
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
	wlr_log(WLR_DEBUG, "override_redirect not handled\n");
}

static void
unmanaged_handle_request_activate(struct wl_listener *listener, void *data)
{
	wlr_log(WLR_DEBUG, "request_activate not handled\n");
}

struct xwayland_unmanaged *
xwayland_unmanaged_create(struct server *server,
			  struct wlr_xwayland_surface *xsurface)
{
	struct xwayland_unmanaged *unmanaged;
	unmanaged = calloc(1, sizeof(struct xwayland_unmanaged));
	unmanaged->server = server;
	unmanaged->xwayland_surface = xsurface;

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

	return unmanaged;
}
