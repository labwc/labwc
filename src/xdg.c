#include "labwc.h"

struct xdg_deco {
	struct wlr_xdg_toplevel_decoration_v1 *wlr_decoration;
	struct server *server;
	struct wl_listener destroy;
	struct wl_listener request_mode;
};

static void xdg_deco_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_deco *xdg_deco =
		wl_container_of(listener, xdg_deco, destroy);
	wl_list_remove(&xdg_deco->destroy.link);
	wl_list_remove(&xdg_deco->request_mode.link);
	free(xdg_deco);
}

static void xdg_deco_request_mode(struct wl_listener *listener, void *data)
{
	struct xdg_deco *xdg_deco;
	xdg_deco = wl_container_of(listener, xdg_deco, request_mode);
	enum wlr_xdg_toplevel_decoration_v1_mode mode;
	if (rc.xdg_shell_server_side_deco)
		mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
	else
		mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
	wlr_xdg_toplevel_decoration_v1_set_mode(xdg_deco->wlr_decoration, mode);
}

void xdg_toplevel_decoration(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, xdg_toplevel_decoration);
	struct wlr_xdg_toplevel_decoration_v1 *wlr_decoration = data;
	struct xdg_deco *xdg_deco = calloc(1, sizeof(struct xdg_deco));
	if (!xdg_deco)
		return;
	xdg_deco->wlr_decoration = wlr_decoration;
	xdg_deco->server = server;
	xdg_deco->destroy.notify = xdg_deco_destroy;
	wl_signal_add(&wlr_decoration->events.destroy, &xdg_deco->destroy);
	xdg_deco->request_mode.notify = xdg_deco_request_mode;
	wl_signal_add(&wlr_decoration->events.request_mode,
		      &xdg_deco->request_mode);
	xdg_deco_request_mode(&xdg_deco->request_mode, wlr_decoration);
}

static bool has_ssd(struct view *view)
{
	if (!rc.xdg_shell_server_side_deco)
		return false;

	/*
	 * Some XDG shells refuse to disable CSD in which case their
	 * geometry.{x,y} seems to be greater. We filter on that on the
	 * assumption that this will remain true.
	 */
	if (view->xdg_surface->geometry.x || view->xdg_surface->geometry.y)
		return false;

	return true;
}

static void handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	BUG_ON(!view->surface);
	view->w = view->surface->current.width;
	view->h = view->surface->current.height;
}

static void handle_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, map);
	view->impl->map(view);
}

static void handle_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, unmap);
	view->impl->unmap(view);
}

static void handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	wl_list_remove(&view->link);
	free(view);
}

static void handle_request_move(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
	 * want. */
	struct view *view = wl_container_of(listener, view, request_move);
	interactive_begin(view, LAB_CURSOR_MOVE, 0);
}

static void handle_request_resize(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
	 * want. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_resize);
	interactive_begin(view, LAB_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_view_configure(struct view *view, struct wlr_box geo)
{
	wlr_xdg_toplevel_set_size(view->xdg_surface, (uint32_t)geo.width,
				  (uint32_t)geo.height);
}

static void xdg_toplevel_view_close(struct view *view)
{
	wlr_xdg_toplevel_send_close(view->xdg_surface);
}

static struct border xdg_shell_border(struct view *view)
{
	struct wlr_box box;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &box);
	struct border border = {
		.top = -box.y,
		.bottom = -box.y,
		.left = -box.x,
		.right = -box.x,
	};
	return border;
}

static bool istopmost(struct view *view)
{
	return view->xdg_surface->toplevel->parent == NULL;
}

static void xdg_toplevel_view_map(struct view *view)
{
	view->mapped = true;
	view->surface = view->xdg_surface->surface;
	if (!view->been_mapped) {
		view->server_side_deco = has_ssd(view);
		if (view->server_side_deco) {
			view->margin = deco_thickness(view);
		} else {
			view->margin = xdg_shell_border(view);
			view->xdg_grab_offset = -view->margin.left;
		}
		if (istopmost(view)) {
			/* align to edge of screen */
			view->x += view->margin.left;
			view->y += view->margin.top;
		}
	}
	view->been_mapped = true;

	wl_signal_add(&view->xdg_surface->surface->events.commit,
		      &view->commit);
	view->commit.notify = handle_commit;

	desktop_focus_view(view);
}

static void xdg_toplevel_view_unmap(struct view *view)
{
	view->mapped = false;
	wl_list_remove(&view->commit.link);
	desktop_focus_next_mapped_view(view);
}

static const struct view_impl xdg_toplevel_view_impl = {
	.configure = xdg_toplevel_view_configure,
	.close = xdg_toplevel_view_close,
	.map = xdg_toplevel_view_map,
	.unmap = xdg_toplevel_view_unmap,
};

void xdg_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		return;

	struct view *view = calloc(1, sizeof(struct view));
	view->server = server;
	view->type = LAB_XDG_SHELL_VIEW;
	view->impl = &xdg_toplevel_view_impl;
	view->xdg_surface = xdg_surface;

	view->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	view->request_move.notify = handle_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = handle_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

	wl_list_insert(&server->views, &view->link);
}
