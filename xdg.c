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
	if (LAB_DISABLE_CSD)
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

void xdg_surface_map(struct wl_listener *listener, void *data)
{
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct view *view = wl_container_of(listener, view, map);
	view->mapped = true;
	view->been_mapped = true;
	view->surface = view->xdg_surface->surface;
	focus_view(view, view->xdg_surface->surface);
}

void xdg_surface_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;
	view_focus_next_toplevel(view->server);
}

void xdg_surface_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	wl_list_remove(&view->link);
	free(view);
}

void xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
	 * want. */
	struct view *view = wl_container_of(listener, view, request_move);
	begin_interactive(view, TINYWL_CURSOR_MOVE, 0);
}

void xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
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
	begin_interactive(view, TINYWL_CURSOR_RESIZE, event->edges);
}

void xdg_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	struct view *view = calloc(1, sizeof(struct view));
	view->server = server;
	view->type = LAB_XDG_SHELL_VIEW;
	view->xdg_surface = xdg_surface;

	view->map.notify = xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

	wl_list_insert(&server->views, &view->link);
}
