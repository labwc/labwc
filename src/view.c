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

void view_minimize(struct view *view)
{
	if (view->minimized == true)
		return;
	view->minimized = true;
	view->impl->unmap(view);
}

void view_unminimize(struct view *view)
{
	if (view->minimized == false)
		return;
	view->minimized = false;
	view->impl->map(view);
}

bool view_hasfocus(struct view *view)
{
	if (!view || !view->surface)
		return false;
	struct wlr_seat *seat = view->server->seat;
	return (view->surface == seat->keyboard_state.focused_surface);
}
