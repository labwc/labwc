#include "labwc.h"
#include "common/bug-on.h"

void view_init_position(struct view *view)
{
	/* If surface already has a 'desired' position, don't touch it */
	if (view->x || view->y)
		return;

	/* Do not touch xwayland surfaces like menus and apps like dmenu */
	if (view->type == LAB_XWAYLAND_VIEW && !view->show_server_side_deco)
		return;

	struct wlr_box box;
	if (view->type == LAB_XDG_SHELL_VIEW && !view->show_server_side_deco)
		/*
		 * We're here either because rc.xml says yes to CSD or
		 * because the XDG shell won't allow CSD to be turned off
		 */
		wlr_xdg_surface_get_geometry(view->xdg_surface, &box);
	else
		box = deco_max_extents(view);
	view->x -= box.x;
	view->y -= box.y;
	if (view->type != LAB_XWAYLAND_VIEW)
		return;
	wlr_xwayland_surface_configure(view->xwayland_surface, view->x, view->y,
				       view->xwayland_surface->width,
				       view->xwayland_surface->height);
}

struct wlr_box view_get_surface_geometry(struct view *view)
{
	struct wlr_box box = { 0 };
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		wlr_xdg_surface_get_geometry(view->xdg_surface, &box);
		break;
	case LAB_XWAYLAND_VIEW:
		box.width = view->w;
		box.height = view->h;
		break;
	}
	return box;
}

/* Get geometry relative to screen */
struct wlr_box view_geometry(struct view *view)
{
	struct wlr_box b = view_get_surface_geometry(view);
	/* Add XDG view invisible border if it exists */
	b.width += 2 * b.x;
	b.height += 2 * b.y;
	/* Make co-ordinates relative to screen */
	b.x = view->x;
	b.y = view->y;
	return b;
}

void view_resize(struct view *view, struct wlr_box geo)
{
	struct wlr_box border = view_get_surface_geometry(view);
	struct wlr_box box = {
		.x = view->x,
		.y = view->y,
		.width = geo.width - 2 * border.x,
		.height = geo.height - 2 * border.y,
	};
	view->impl->configure(view, box);
}

static void move_to_front(struct view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
}

/* Activate/deactivate toplevel surface */
static void set_activated(struct wlr_surface *surface, bool activated)
{
	if (!surface)
		return;
	if (wlr_surface_is_xdg_surface(surface)) {
		struct wlr_xdg_surface *previous;
		previous = wlr_xdg_surface_from_wlr_surface(surface);
		wlr_xdg_toplevel_set_activated(previous, activated);
	} else {
		struct wlr_xwayland_surface *previous;
		previous = wlr_xwayland_surface_from_wlr_surface(surface);
		wlr_xwayland_surface_activate(previous, activated);
	}
}

bool view_hasfocus(struct view *view)
{
	if (!view || !view->surface)
		return false;
	struct wlr_seat *seat = view->server->seat;
	return (view->surface == seat->keyboard_state.focused_surface);
}

static struct wlr_xwayland_surface *top_parent_of(struct view *view)
{
	struct wlr_xwayland_surface *s = view->xwayland_surface;
	while (s->parent)
		s = s->parent;
	return s;
}

static void move_xwayland_sub_views_to_front(struct view *parent)
{
	if (!parent || parent->type != LAB_XWAYLAND_VIEW)
		return;
	struct view *view, *next;
	wl_list_for_each_reverse_safe(view, next, &parent->server->views, link)
	{
		/* need to stop here, otherwise loops keeps going forever */
		if (view == parent)
			break;
		if (view->type != LAB_XWAYLAND_VIEW)
			continue;
		if (!view->mapped && !view->minimized)
			continue;
		if (top_parent_of(view) != parent->xwayland_surface)
			continue;
		move_to_front(view);
		/* TODO: we should probably focus on these too here */
	}
}

void view_focus(struct view *view)
{
	/* Note: this function only deals with keyboard focus. */
	if (!view)
		return;

	/* TODO: messy - sort out */
	if (!view->mapped) {
		view->impl->map(view);
		return;
	}

	struct server *server = view->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface;
	prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == view->surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (view->type == LAB_XWAYLAND_VIEW) {
		/* Don't focus on menus, etc */
		move_to_front(view);
		if (!wlr_xwayland_or_surface_wants_focus(
			    view->xwayland_surface)) {
			return;
		}
	}
	if (prev_surface)
		set_activated(seat->keyboard_state.focused_surface, false);

	move_to_front(view);
	set_activated(view->surface, true);
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	wlr_seat_keyboard_notify_enter(seat, view->surface, keyboard->keycodes,
				       keyboard->num_keycodes,
				       &keyboard->modifiers);

	move_xwayland_sub_views_to_front(view);
}

static struct view *first_view(struct server *server)
{
	struct view *view;
	view = wl_container_of(server->views.next, view, link);
	return view;
}

/*
 * Some xwayland apps produce unmapped surfaces on startup and also leave
 * some unmapped surfaces kicking around on 'close' (for example * leafpad's
 * "about" dialogue). Whilst this is not normally a problem, we have to be
 * careful when cycling between views. The only view's we should focus are
 * those that are already mapped and those that have been minimized.
 */
static bool isfocusable(struct view *view)
{
	/* filter out those xwayland surfaces that have never been mapped */
	if (!view->surface)
		return false;
	return (view->mapped || view->minimized);
}

static int has_focusable_view(struct wl_list *wl_list)
{
	struct view *view;
	wl_list_for_each (view, wl_list, link) {
		if (isfocusable(view))
			return true;
	}
	return false;
}

/**
 * view_next - return next view
 * @current: view used as reference point for defining 'next'
 * Note: If @current=NULL, the list's second view is returned
 */
struct view *view_next(struct server *server, struct view *current)
{
	if (!has_focusable_view(&server->views))
		return NULL;

	struct view *view = current ? current : first_view(server);

	/* Replacement for wl_list_for_each_from() */
	do {
		view = wl_container_of(view->link.next, view, link);
	} while (&view->link == &server->views || !isfocusable(view));
	return view;
}

static bool _view_at(struct view *view, double lx, double ly,
		     struct wlr_surface **surface, double *sx, double *sy)
{
	/*
	 * XDG toplevels may have nested surfaces, such as popup windows for
	 * context menus or tooltips. This function tests if any of those are
	 * underneath the coordinates lx and ly (in output Layout Coordinates).
	 * If so, it sets the surface pointer to that wlr_surface and the sx and
	 * sy coordinates to the coordinates relative to that surface's top-left
	 * corner.
	 */
	double view_sx = lx - view->x;
	double view_sy = ly - view->y;
	double _sx, _sy;
	struct wlr_surface *_surface = NULL;

	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		_surface = wlr_xdg_surface_surface_at(
			view->xdg_surface, view_sx, view_sy, &_sx, &_sy);
		break;
	case LAB_XWAYLAND_VIEW:
		if (!view->xwayland_surface->surface)
			return false;
		_surface =
			wlr_surface_surface_at(view->xwayland_surface->surface,
					       view_sx, view_sy, &_sx, &_sy);
		break;
	}

	if (_surface) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}
	return false;
}

struct view *view_at(struct server *server, double lx, double ly,
		     struct wlr_surface **surface, double *sx, double *sy,
		     int *view_area)
{
	/*
	 * This iterates over all of our surfaces and attempts to find one under
	 * the cursor. It relies on server->views being ordered from
	 * top-to-bottom.
	 */
	struct view *view;
	wl_list_for_each (view, &server->views, link) {
		if (_view_at(view, lx, ly, surface, sx, sy))
			return view;
		if (!view->show_server_side_deco)
			continue;
		*view_area = deco_at(view, lx, ly);
		if (*view_area != LAB_DECO_NONE)
			return view;
	}
	return NULL;
}
