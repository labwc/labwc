#include "labwc.h"

struct wlr_box view_get_surface_geometry(struct view *view)
{
	struct wlr_box box = { 0 };
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		wlr_xdg_surface_get_geometry(view->xdg_surface, &box);
		break;
	case LAB_XWAYLAND_VIEW:
		box.width = view->xwayland_surface->width;
		box.height = view->xwayland_surface->height;
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
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		wlr_xdg_toplevel_set_size(view->xdg_surface,
					  geo.width - 2 * border.x,
					  geo.height - 2 * border.y);
		break;
	case LAB_XWAYLAND_VIEW:
		wlr_xwayland_surface_configure(view->xwayland_surface, view->x,
					       view->y, geo.width, geo.height);
		break;
	default:
		break;
	}
}

static bool is_toplevel(struct view *view)
{
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		return view->xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL;
	case LAB_XWAYLAND_VIEW:
		return xwl_nr_parents(view) > 0 ? false : true;
	}
	return false;
}

bool view_want_deco(struct view *view)
{
	if (!is_toplevel(view))
		return false;
	if (view->type == LAB_XDG_SHELL_VIEW && !LAB_DISABLE_CSD)
		return false;
	if (view->type == LAB_XDG_SHELL_VIEW)
		return true;
	if (view->xwayland_surface->override_redirect)
		return false;
	if (view->xwayland_surface->decorations !=
	    WLR_XWAYLAND_SURFACE_DECORATIONS_ALL)
		return false;
	return true;
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

void view_focus(struct view *view)
{
	/* Note: this function only deals with keyboard focus. */
	if (!view || !view->surface)
		return;
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

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	move_to_front(view);
	set_activated(view->surface, true);
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will
	 * keep track of this and automatically send key events to the
	 * appropriate clients without additional work on your part.
	 */
	wlr_seat_keyboard_notify_enter(seat, view->surface, keyboard->keycodes,
				       keyboard->num_keycodes,
				       &keyboard->modifiers);
}

struct view *view_front_toplevel(struct server *server)
{
	struct view *view;
	wl_list_for_each (view, &server->views, link) {
		if (!view->been_mapped)
			continue;
		if (is_toplevel(view))
			return view;
	}
	return NULL;
}

struct view *next_toplevel(struct view *current)
{
	/* FIXME: write nr_toplevels() */
	struct view *view = current;
	do {
		view = wl_container_of(view->link.next, view, link);
	} while (!view->been_mapped || !is_toplevel(view));
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
		if (!view_want_deco(view))
			continue;
		if (deco_at(view, lx, ly) == LAB_DECO_PART_TITLE) {
			*view_area = LAB_DECO_PART_TITLE;
			return view;
		}
		if (deco_at(view, lx, ly) == LAB_DECO_PART_TOP) {
			*view_area = LAB_DECO_PART_TOP;
			return view;
		}
		if (deco_at(view, lx, ly) == LAB_DECO_PART_RIGHT) {
			*view_area = LAB_DECO_PART_RIGHT;
			return view;
		}
		if (deco_at(view, lx, ly) == LAB_DECO_PART_BOTTOM) {
			*view_area = LAB_DECO_PART_BOTTOM;
			return view;
		}
		if (deco_at(view, lx, ly) == LAB_DECO_PART_LEFT) {
			*view_area = LAB_DECO_PART_LEFT;
			return view;
		}
	}
	return NULL;
}
