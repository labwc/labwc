#include "labwc.h"
#include "rcxml.h"

static void show_one_xdg_view(struct view *view)
{
	fprintf(stderr, "XDG  ");
	switch (view->xdg_surface->role) {
	case WLR_XDG_SURFACE_ROLE_NONE:
		fprintf(stderr, "- ");
		break;
	case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
		fprintf(stderr, "0 ");
		break;
	case WLR_XDG_SURFACE_ROLE_POPUP:
		fprintf(stderr, "? ");
		break;
	}
	fprintf(stderr, "                   %p %s", (void *)view,
		view->xdg_surface->toplevel->app_id);
	fprintf(stderr, "  {%d, %d, %d, %d}\n", view->xdg_surface->geometry.x,
		view->xdg_surface->geometry.y,
		view->xdg_surface->geometry.height,
		view->xdg_surface->geometry.width);
}

static void show_one_xwl_view(struct view *view)
{
	fprintf(stderr, "XWL  ");
	if (!view->been_mapped) {
		fprintf(stderr, "- ");
	} else {
		fprintf(stderr, "%d ", xwl_nr_parents(view));
	}
	fprintf(stderr, "     %d      ",
		wl_list_length(&view->xwayland_surface->children));
	if (view->mapped) {
		fprintf(stderr, "Y");
	} else {
		fprintf(stderr, "-");
	}
	fprintf(stderr, "      %p %s {%d,%d,%d,%d}\n", (void *)view,
		view->xwayland_surface->class, view->xwayland_surface->x,
		view->xwayland_surface->y, view->xwayland_surface->width,
		view->xwayland_surface->height);
	/*
	 * Other variables to consider printing:
	 *
	 * view->mapped,
	 * view->been_mapped,
	 * view->xwayland_surface->override_redirect,
	 * wlr_xwayland_or_surface_wants_focus(view->xwayland_surface));
	 * view->xwayland_surface->saved_width,
	 * view->xwayland_surface->saved_height);
	 * view->xwayland_surface->surface->sx,
	 * view->xwayland_surface->surface->sy);
	 */
}

void dbg_show_one_view(struct view *view)
{
	if (view->type == LAB_XDG_SHELL_VIEW)
		show_one_xdg_view(view);
	else if (view->type == LAB_XWAYLAND_VIEW)
		show_one_xwl_view(view);
}

void dbg_show_views(struct server *server)
{
	struct view *view;

	fprintf(stderr, "---\n");
	fprintf(stderr, "TYPE NR_PNT NR_CLD MAPPED VIEW-POINTER   NAME\n");
	wl_list_for_each_reverse (view, &server->views, link)
		dbg_show_one_view(view);
}

void dbg_show_keybinds()
{
	struct keybind *keybind;
	wl_list_for_each_reverse (keybind, &rc.keybinds, link) {
		printf("KEY=%s-", keybind->action);
		for (size_t i = 0; i < keybind->keysyms_len; i++)
			printf("    %d\n", keybind->keysyms[i]);
	}
}
