#include "config.h"
#include <assert.h>
#include "labwc.h"
#include "ssd.h"

static void
move_to_front(struct view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
}

#if HAVE_XWAYLAND
static struct wlr_xwayland_surface *
top_parent_of(struct view *view)
{
	struct wlr_xwayland_surface *s = view->xwayland_surface;
	while (s->parent) {
		s = s->parent;
	}
	return s;
}

static void
move_xwayland_sub_views_to_front(struct view *parent)
{
	if (!parent || parent->type != LAB_XWAYLAND_VIEW) {
		return;
	}
	struct view *view, *next;
	wl_list_for_each_reverse_safe(view, next, &parent->server->views, link)
	{
		/* need to stop here, otherwise loops keeps going forever */
		if (view == parent) {
			break;
		}
		if (view->type != LAB_XWAYLAND_VIEW) {
			continue;
		}
		if (!view->mapped && !view->minimized) {
			continue;
		}
		if (top_parent_of(view) != parent->xwayland_surface) {
			continue;
		}
		move_to_front(view);
		/* TODO: we should probably focus on these too here */
	}
}
#endif

/* Activate/deactivate toplevel surface */
static void
set_activated(struct wlr_surface *surface, bool activated)
{
	if (!surface) {
		return;
	}
	if (wlr_surface_is_xdg_surface(surface)) {
		struct wlr_xdg_surface *s;
		s = wlr_xdg_surface_from_wlr_surface(surface);
		wlr_xdg_toplevel_set_activated(s, activated);
#if HAVE_XWAYLAND
	} else if (wlr_surface_is_xwayland_surface(surface)) {
		struct wlr_xwayland_surface *s;
		s = wlr_xwayland_surface_from_wlr_surface(surface);
		wlr_xwayland_surface_activate(s, activated);
#endif
	}
}

void
desktop_focus_view(struct seat *seat, struct view *view)
{
	if (!view) {
		seat_focus_surface(seat, NULL);
		return;
	}
	if (view->minimized) {
		/* this will unmap and then focus */
		view_unminimize(view);
		return;
	} else if (view->mapped) {
		struct wlr_surface *prev_surface;
		prev_surface = seat->seat->keyboard_state.focused_surface;
		if (prev_surface == view->surface) {
			/* Don't re-focus an already focused surface. */
			return;
		}
		if (prev_surface) {
			set_activated(prev_surface, false);
		}
		move_to_front(view);
		set_activated(view->surface, true);
		seat_focus_surface(seat, view->surface);
#if HAVE_XWAYLAND
		move_xwayland_sub_views_to_front(view);
#endif
	}
}

/*
 * Some xwayland apps produce unmapped surfaces on startup and also leave
 * some unmapped surfaces kicking around on 'close' (for example * leafpad's
 * "about" dialogue). Whilst this is not normally a problem, we have to be
 * careful when cycling between views. The only views we should focus are
 * those that are already mapped and those that have been minimized.
 */
static bool
isfocusable(struct view *view)
{
	/* filter out those xwayland surfaces that have never been mapped */
	if (!view->surface) {
		return false;
	}
	return (view->mapped || view->minimized);
}

static bool
has_focusable_view(struct wl_list *wl_list)
{
	struct view *view;
	wl_list_for_each (view, wl_list, link) {
		if (isfocusable(view)) {
			return true;
		}
	}
	return false;
}

static struct view *
first_view(struct server *server)
{
	struct view *view;
	view = wl_container_of(server->views.next, view, link);
	return view;
}

struct view *
desktop_cycle_view(struct server *server, struct view *current)
{
	if (!has_focusable_view(&server->views)) {
		return NULL;
	}

	struct view *view = current ? current : first_view(server);

	/* Replacement for wl_list_for_each_from() */
	do {
		view = wl_container_of(view->link.next, view, link);
	} while (&view->link == &server->views || !isfocusable(view));
	damage_all_outputs(server);
	return view;
}

static bool
has_mapped_view(struct wl_list *wl_list)
{
	struct view *view;
	wl_list_for_each (view, wl_list, link) {
		if (view->mapped) {
			return true;
		}
	}
	return false;
}

static struct view *
topmost_mapped_view(struct server *server)
{
	if (!has_mapped_view(&server->views)) {
		return NULL;
	}

	/* start from tail of server->views */
	struct view *view = wl_container_of(server->views.prev, view, link);
	do {
		view = wl_container_of(view->link.next, view, link);
	} while (&view->link == &server->views || !view->mapped);
	return view;
}

void
desktop_focus_topmost_mapped_view(struct server *server)
{
	struct view *view = topmost_mapped_view(server);
	desktop_focus_view(&server->seat, view);
}

static bool
_view_at(struct view *view, double lx, double ly, struct wlr_surface **surface,
	 double *sx, double *sy)
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
#if HAVE_XWAYLAND
	case LAB_XWAYLAND_VIEW:
		_surface = wlr_surface_surface_at(view->surface, view_sx,
						  view_sy, &_sx, &_sy);
		break;
#endif
	}

	if (_surface) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}
	return false;
}

struct view *
desktop_view_at(struct server *server, double lx, double ly,
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
		if (!view->mapped) {
			continue;
		}
		if (_view_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
		if (!view->ssd.enabled) {
			continue;
		}
		*view_area = ssd_at(view, lx, ly);
		if (*view_area != LAB_SSD_NONE) {
			return view;
		}
	}
	return NULL;
}
