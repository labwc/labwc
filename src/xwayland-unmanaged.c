#include "labwc.h"

static void unmanaged_handle_request_configure(struct wl_listener *listener,
					       void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, request_configure);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	struct wlr_xwayland_surface_configure_event *ev = data;
	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y, ev->width,
				       ev->height);
}

static void unmanaged_handle_commit(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, commit);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	unmanaged->lx = xsurface->x;
	unmanaged->ly = xsurface->y;
}

static void unmanaged_handle_map(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, map);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;

	wl_list_insert(unmanaged->server->unmanaged_surfaces.prev,
		       &unmanaged->link);

	wl_signal_add(&xsurface->surface->events.commit, &unmanaged->commit);
	unmanaged->commit.notify = unmanaged_handle_commit;

	unmanaged->lx = xsurface->x;
	unmanaged->ly = xsurface->y;

	if (wlr_xwayland_or_surface_wants_focus(xsurface))
		seat_focus_surface(xsurface->surface);
}

static void unmanaged_handle_unmap(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, unmap);
	struct wlr_xwayland_surface *xsurface = unmanaged->xwayland_surface;
	wl_list_remove(&unmanaged->link);
	wl_list_remove(&unmanaged->commit.link);

	if (seat_focused_surface() == xsurface->surface) {
		struct xwayland_unmanaged *u;
		struct wl_list *list = &unmanaged->server->unmanaged_surfaces;
		wl_list_for_each (u, list, link) {
			struct wlr_xwayland_surface *prev = u->xwayland_surface;
			if (!wlr_xwayland_or_surface_wants_focus(prev))
				continue;
			seat_focus_surface(prev->surface);
			return;
		}
	}
}

static void unmanaged_handle_destroy(struct wl_listener *listener, void *data)
{
	struct xwayland_unmanaged *unmanaged =
		wl_container_of(listener, unmanaged, destroy);
	wl_list_remove(&unmanaged->map.link);
	wl_list_remove(&unmanaged->unmap.link);
	wl_list_remove(&unmanaged->destroy.link);
	free(unmanaged);
}

void xwayland_unmanaged_create(struct server *server,
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
}
