#include "labwc.h"

int xwl_nr_parents(struct view *view)
{
	struct wlr_xwayland_surface *s = view->xwayland_surface;
	int i = 0;

	if (!s) {
		fprintf(stderr, "warn: (%s) no xwayland surface\n", __func__);
		return -1;
	}
	while (s->parent) {
		s = s->parent;
		++i;
	}
	return i;
}

static void position(struct view *view)
{
	struct wlr_box box;
	if (!view_want_deco(view))
		return;
	if (view->x || view->y)
		return;
	box = deco_box(view, LAB_DECO_PART_TOP);
	view->y = box.height;
	box = deco_box(view, LAB_DECO_PART_LEFT);
	view->x = box.width;
	wlr_xwayland_surface_configure(view->xwayland_surface, view->x, view->y,
				       view->xwayland_surface->width,
				       view->xwayland_surface->height);
}

void xwl_surface_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, map);
	view->mapped = true;
	view->x = view->xwayland_surface->x;
	view->y = view->xwayland_surface->y;
	view->surface = view->xwayland_surface->surface;
	if (!view->been_mapped)
		position(view);
	view->been_mapped = true;
	view_focus(view);
}

void xwl_surface_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;
	view_focus_next_toplevel(view);
}

void xwl_surface_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	wl_list_remove(&view->link);
	free(view);
}

void xwl_surface_configure(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	wlr_xwayland_surface_configure(view->xwayland_surface, event->x,
				       event->y, event->width, event->height);
}

void xwl_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xwayland_surface);
	struct wlr_xwayland_surface *xwayland_surface = data;
	wlr_xwayland_surface_ping(xwayland_surface);

	struct view *view = calloc(1, sizeof(struct view));
	view->server = server;
	view->type = LAB_XWAYLAND_VIEW;
	view->xwayland_surface = xwayland_surface;

	view->map.notify = xwl_surface_map;
	wl_signal_add(&xwayland_surface->events.map, &view->map);
	view->unmap.notify = xwl_surface_unmap;
	wl_signal_add(&xwayland_surface->events.unmap, &view->unmap);
	view->destroy.notify = xwl_surface_destroy;
	wl_signal_add(&xwayland_surface->events.destroy, &view->destroy);
	view->request_configure.notify = xwl_surface_configure;
	wl_signal_add(&xwayland_surface->events.request_configure,
		      &view->request_configure);

	wl_list_insert(&server->views, &view->link);
}
