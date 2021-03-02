#include <assert.h>
#include "labwc.h"

/*
 * xdg_popup_create() and subsurface_create() are only called for the
 * purposes of tracking damage.
 */
static void
handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	xdg_popup_create(view, wlr_popup);
}

static void
new_subsurface_notify(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, new_subsurface);
	struct wlr_subsurface *wlr_subsurface = data;
	subsurface_create(view, wlr_subsurface);
}

static bool
has_ssd(struct view *view)
{
	if (!rc.xdg_shell_server_side_deco) {
		return false;
	}

	/*
	 * Some XDG shells refuse to disable CSD in which case their
	 * geometry.{x,y} seems to be greater than zero. We filter on that
	 * on the assumption that this will remain true.
	 */
	if (view->xdg_surface->geometry.x || view->xdg_surface->geometry.y) {
		return false;
	}
	return true;
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	assert(view->surface);
	struct wlr_box size;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &size);

	view->w = size.width;
	view->h = size.height;

	uint32_t serial = view->pending_move_resize.configure_serial;
	if (serial > 0 && serial >= view->xdg_surface->configure_serial) {
		if (view->pending_move_resize.update_x) {
			view->x = view->pending_move_resize.x +
				view->pending_move_resize.width - size.width;
		}
		if (view->pending_move_resize.update_y) {
			view->y = view->pending_move_resize.y +
				view->pending_move_resize.height - size.height;
		}
		if (serial == view->xdg_surface->configure_serial) {
			view->pending_move_resize.configure_serial = 0;
		}
	}
	damage_view_part(view);
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
	free(view);
}

static void
handle_request_move(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
	 * want. */
	struct view *view = wl_container_of(listener, view, request_move);
	interactive_begin(view, LAB_INPUT_STATE_MOVE, 0);
}

static void
handle_request_resize(struct wl_listener *listener, void *data)
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
	interactive_begin(view, LAB_INPUT_STATE_RESIZE, event->edges);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they want.
	 */

	struct view *view = wl_container_of(listener, view, request_maximize);

	struct wlr_xdg_surface *surface = data;
	if(view != NULL) {
		view_maximize(view, surface->toplevel->client_pending.maximized);
	}

}

static void
xdg_toplevel_view_configure(struct view *view, struct wlr_box geo)
{
	view->pending_move_resize.update_x = geo.x != view->x;
	view->pending_move_resize.update_y = geo.y != view->y;
	view->pending_move_resize.x = geo.x;
	view->pending_move_resize.y = geo.y;
	view->pending_move_resize.width = geo.width;
	view->pending_move_resize.height = geo.height;

	uint32_t serial = wlr_xdg_toplevel_set_size(view->xdg_surface,
		(uint32_t)geo.width, (uint32_t)geo.height);
	if (serial > 0) {
		view->pending_move_resize.configure_serial = serial;
	} else if (view->pending_move_resize.configure_serial == 0) {
		view->x = geo.x;
		view->y = geo.y;
		damage_all_outputs(view->server);
	}
}

static void
xdg_toplevel_view_move(struct view *view, double x, double y)
{
	view->x = x;
	view->y = y;
	damage_all_outputs(view->server);
}

static void
xdg_toplevel_view_close(struct view *view)
{
	wlr_xdg_toplevel_send_close(view->xdg_surface);
}

static void
xdg_toplevel_view_for_each_popup(struct view *view,
		wlr_surface_iterator_func_t iterator, void *data)
{
	wlr_xdg_surface_for_each_popup(view->xdg_surface, iterator, data);
}

static void
xdg_toplevel_view_for_each_surface(struct view *view,
		wlr_surface_iterator_func_t iterator, void *data)
{
	wlr_xdg_surface_for_each_surface(view->xdg_surface, iterator, data);
}

static void
xdg_toplevel_view_maximize(struct view *view, bool maximized)
{
	wlr_xdg_toplevel_set_maximized(view->xdg_surface, maximized);
}

/* Return area between surface extremities and window */
static struct border
xdg_shell_padding(struct view *view)
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

static bool
istopmost(struct view *view)
{
	return view->xdg_surface->toplevel->parent == NULL;
}

static void
xdg_toplevel_view_map(struct view *view)
{
	view->mapped = true;
	view->surface = view->xdg_surface->surface;
	if (!view->been_mapped) {
		view->server_side_deco = has_ssd(view);
		if (view->server_side_deco) {
			view->margin = deco_thickness(view);
		} else {
			view->padding = xdg_shell_padding(view);
		}
		if (istopmost(view)) {
			/* align to edge of screen */
			view->x += view->margin.left;
			view->y += view->margin.top;
			view->x += view->padding.left;
			view->y += view->padding.top;
		}
	}
	view->been_mapped = true;

	view->commit.notify = handle_commit;
	wl_signal_add(&view->xdg_surface->surface->events.commit,
		&view->commit);
	view->new_subsurface.notify = new_subsurface_notify;
	wl_signal_add(&view->surface->events.new_subsurface,
		&view->new_subsurface);

	desktop_focus_view(&view->server->seat, view);
	damage_all_outputs(view->server);
}

static void
xdg_toplevel_view_unmap(struct view *view)
{
	view->mapped = false;
	damage_all_outputs(view->server);
	wl_list_remove(&view->commit.link);
	wl_list_remove(&view->new_subsurface.link);
	desktop_focus_topmost_mapped_view(view->server);
}

static const struct view_impl xdg_toplevel_view_impl = {
	.configure = xdg_toplevel_view_configure,
	.close = xdg_toplevel_view_close,
	.for_each_popup = xdg_toplevel_view_for_each_popup,
	.for_each_surface = xdg_toplevel_view_for_each_surface,
	.map = xdg_toplevel_view_map,
	.move = xdg_toplevel_view_move,
	.unmap = xdg_toplevel_view_unmap,
	.maximize = xdg_toplevel_view_maximize
};

void
xdg_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}
	wlr_xdg_surface_ping(xdg_surface);

	struct view *view = calloc(1, sizeof(struct view));
	view->server = server;
	view->type = LAB_XDG_SHELL_VIEW;
	view->impl = &xdg_toplevel_view_impl;
	view->xdg_surface = xdg_surface;
	view->maximized = false;

	view->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	view->new_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&xdg_surface->events.new_popup, &view->new_popup);

	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	view->request_move.notify = handle_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = handle_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);

	// hacky workaround to bug where sometimes a window starts maximized and
	// when we hit the maximize button, labwc crashes. instead we unmaximize
	// it from the start to avoid this situation
	view_maximize(view, false);

	wl_list_insert(&server->views, &view->link);
}
