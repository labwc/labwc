#include <assert.h>
#include "labwc.h"
#include "ssd.h"

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	assert(view->surface);

	/* Must receive commit signal before accessing surface->current* */
	view->w = view->surface->current.width;
	view->h = view->surface->current.height;

	if (view->pending_move_resize.update_x) {
		view->x = view->pending_move_resize.x +
			view->pending_move_resize.width - view->w;
		view->pending_move_resize.update_x = false;
	}
	if (view->pending_move_resize.update_y) {
		view->y = view->pending_move_resize.y +
			view->pending_move_resize.height - view->h;
		view->pending_move_resize.update_y = false;
	}
	ssd_update_geometry(view);
	damage_view_whole(view);
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
	wl_list_remove(&view->request_maximize.link);
	ssd_destroy(view);
	free(view);
}

static void
handle_request_configure(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	wlr_xwayland_surface_configure(view->xwayland_surface, event->x,
				       event->y, event->width, event->height);
	damage_all_outputs(view->server);
}

static void handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_maximize);

	if(view != NULL) {
		view_maximize(view, !view->maximized);
	}

}

static void
configure(struct view *view, struct wlr_box geo)
{
	view->pending_move_resize.update_x = geo.x != view->x;
	view->pending_move_resize.update_y = geo.y != view->y;
	view->pending_move_resize.x = geo.x;
	view->pending_move_resize.y = geo.y;
	view->pending_move_resize.width = geo.width;
	view->pending_move_resize.height = geo.height;
	wlr_xwayland_surface_configure(view->xwayland_surface, (int16_t)geo.x,
				       (int16_t)geo.y, (uint16_t)geo.width,
				       (uint16_t)geo.height);
	damage_all_outputs(view->server);
}

static void
move(struct view *view, double x, double y)
{
	view->x = x;
	view->y = y;
	struct wlr_xwayland_surface *s = view->xwayland_surface;
	wlr_xwayland_surface_configure(s, (int16_t)x, (int16_t)y,
		(uint16_t)s->width, (uint16_t)s->height);
	ssd_update_geometry(view);
	damage_all_outputs(view->server);
}

static void
_close(struct view *view)
{
	wlr_xwayland_surface_close(view->xwayland_surface);
	damage_all_outputs(view->server);
}

static void
for_each_surface(struct view *view, wlr_surface_iterator_func_t iterator,
		void *data)
{
	if (!view->surface) {
		return;
	}
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
	view->mapped = true;
	view->x = view->xwayland_surface->x;
	view->y = view->xwayland_surface->y;
	view->w = view->xwayland_surface->width;
	view->h = view->xwayland_surface->height;
	view->surface = view->xwayland_surface->surface;
	view->ssd.enabled = want_deco(view);

	if (view->ssd.enabled) {
		view->margin = ssd_thickness(view);
		ssd_create(view);
	}

	if (!view->been_mapped) {
		view_maximize(view, false);
		struct wlr_box *box = output_box_from_cursor_coords(view->server);
		view->x = box->x;
		view->y = box->y;
		view_center(view);
		view->been_mapped = true;
	}

	top_left_edge_boundary_check(view);

	/* Add commit here, as xwayland map/unmap can change the wlr_surface */
	wl_signal_add(&view->xwayland_surface->surface->events.commit,
		      &view->commit);
	view->commit.notify = handle_commit;

	desktop_focus_view(&view->server->seat, view);
	damage_all_outputs(view->server);
}

static void
unmap(struct view *view)
{
	view->mapped = false;
	damage_all_outputs(view->server);
	wl_list_remove(&view->commit.link);
	desktop_focus_topmost_mapped_view(view->server);
}

static void
maximize(struct view *view, bool maximized)
{
	wlr_xwayland_surface_set_maximized(view->xwayland_surface, maximized);
}

static const struct view_impl xwl_view_impl = {
	.configure = configure,
	.close = _close,
	.for_each_surface = for_each_surface,
	.map = map,
	.move = move,
	.unmap = unmap,
	.maximize = maximize
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
	wl_list_init(&view->ssd.parts);

	view->map.notify = handle_map;
	wl_signal_add(&xsurface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xsurface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xsurface->events.destroy, &view->destroy);
	view->request_configure.notify = handle_request_configure;
	wl_signal_add(&xsurface->events.request_configure,
		      &view->request_configure);
	view->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&xsurface->events.request_maximize, &view->request_maximize);

	wl_list_insert(&view->server->views, &view->link);
}
