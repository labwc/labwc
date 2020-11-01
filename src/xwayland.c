#include <assert.h>
#include "labwc.h"

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	assert(view->surface);

	/* Must receive commit signal before accessing surface->current* */
	view->w = view->surface->current.width;
	view->h = view->surface->current.height;
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
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	wl_list_remove(&view->link);
	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_configure.link);
	free(view);
}

static void
handle_request_configure(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	wlr_xwayland_surface_configure(view->xwayland_surface, event->x,
				       event->y, event->width, event->height);
}

static void
configure(struct view *view, struct wlr_box geo)
{
	wlr_xwayland_surface_configure(view->xwayland_surface, (int16_t)geo.x,
				       (int16_t)geo.y, (uint16_t)geo.width,
				       (uint16_t)geo.height);
}

static void
_close(struct view *view)
{
	wlr_xwayland_surface_close(view->xwayland_surface);
}

static void
for_each_surface(struct view *view, wlr_surface_iterator_func_t iterator,
		void *data)
{
        wlr_surface_for_each_surface(view->surface, iterator, data);
}

static bool
want_deco(struct view *view)
{
	return view->xwayland_surface->decorations ==
	       WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
}

static void
top_left_edge_boundary_check(struct view *view)
{
	struct wlr_box deco = deco_max_extents(view);
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
	view->mapped = true;
	view->x = view->xwayland_surface->x;
	view->y = view->xwayland_surface->y;
	view->w = view->xwayland_surface->width;
	view->h = view->xwayland_surface->height;
	view->surface = view->xwayland_surface->surface;
	view->server_side_deco = want_deco(view);

	view->margin = deco_thickness(view);

	top_left_edge_boundary_check(view);

	/* Add commit here, as xwayland map/unmap can change the wlr_surface */
	wl_signal_add(&view->xwayland_surface->surface->events.commit,
		      &view->commit);
	view->commit.notify = handle_commit;

	desktop_focus_view(&view->server->seat, view);
}

static void
unmap(struct view *view)
{
	view->mapped = false;
	wl_list_remove(&view->commit.link);
	desktop_focus_topmost_mapped_view(view->server);
}

static const struct view_impl xwl_view_impl = {
	.configure = configure,
	.close = _close,
	.for_each_surface = for_each_surface,
	.map = map,
	.unmap = unmap,
};

void
xwayland_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xwayland_surface);
	struct wlr_xwayland_surface *xsurface = data;
	wlr_xwayland_surface_ping(xsurface);

	/*
	 * We do not create 'views' for xwayland override_redirect surfaces,
	 * but add them to server.unmanaged_surfaces so that we can render them
	 */
	if (xsurface->override_redirect) {
		xwayland_unmanaged_create(server, xsurface);
		return;
	}

	struct view *view = calloc(1, sizeof(struct view));
	view->server = server;
	view->type = LAB_XWAYLAND_VIEW;
	view->impl = &xwl_view_impl;
	view->xwayland_surface = xsurface;

	view->map.notify = handle_map;
	wl_signal_add(&xsurface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xsurface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xsurface->events.destroy, &view->destroy);
	view->request_configure.notify = handle_request_configure;
	wl_signal_add(&xsurface->events.request_configure,
		      &view->request_configure);

	wl_list_insert(&view->server->views, &view->link);
}
