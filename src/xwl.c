#include "labwc.h"
#include "common/log.h"
#include "common/bug-on.h"

static bool has_ssd(struct view *view)
{
	if (view->xwayland_surface->override_redirect)
		return false;
	if (view->xwayland_surface->decorations !=
	    WLR_XWAYLAND_SURFACE_DECORATIONS_ALL)
		return false;
	return true;
}

static void handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	BUG_ON(!view->surface);

	/* Must receive commit signal before accessing surface->current* */
	view->w = view->surface->current.width;
	view->h = view->surface->current.height;
}

void xwl_surface_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, map);
	view->impl->map(view);
}

void xwl_surface_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, unmap);
	view->impl->unmap(view);
}

void xwl_surface_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	if (view->been_mapped)
		wl_list_remove(&view->link);
	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_configure.link);
	free(view);
}

void xwl_surface_configure(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	wlr_xwayland_surface_configure(view->xwayland_surface, event->x,
				       event->y, event->width, event->height);
}

static void xwl_view_configure(struct view *view, struct wlr_box geo)
{
	wlr_xwayland_surface_configure(view->xwayland_surface, (int16_t)geo.x,
				       (int16_t)geo.y, (uint16_t)geo.width,
				       (uint16_t)geo.height);
}

static void xwl_view_close(struct view *view)
{
	wlr_xwayland_surface_close(view->xwayland_surface);
}

static void xwl_view_map(struct view *view)
{
	view->mapped = true;
	view->x = view->xwayland_surface->x;
	view->y = view->xwayland_surface->y;
	view->surface = view->xwayland_surface->surface;
	if (!view->been_mapped) {
		view->show_server_side_deco = has_ssd(view);
		view_init_position(view);
		wl_list_insert(&view->server->views, &view->link);
	}
	view->been_mapped = true;

	/*
	 * Add commit listener here, because xwayland map/unmap can change
	 * the wlr_surface
	 */
	wl_signal_add(&view->xwayland_surface->surface->events.commit,
		      &view->commit);
	view->commit.notify = handle_commit;

	view_focus(view);
}

static void xwl_view_unmap(struct view *view)
{
	view->mapped = false;
	wl_list_remove(&view->commit.link);
	view_focus(next_toplevel(view));
}

static const struct view_impl xwl_view_impl = {
	.configure = xwl_view_configure,
	.close = xwl_view_close,
	.map = xwl_view_map,
	.unmap = xwl_view_unmap,
};

void xwl_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xwayland_surface);
	struct wlr_xwayland_surface *xwayland_surface = data;
	wlr_xwayland_surface_ping(xwayland_surface);

	struct view *view = calloc(1, sizeof(struct view));
	view->server = server;
	view->type = LAB_XWAYLAND_VIEW;
	view->impl = &xwl_view_impl;
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
}
