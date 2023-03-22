// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_server_decoration.h>
#include "common/list.h"
#include "common/mem.h"
#include "decorations.h"
#include "labwc.h"
#include "view.h"

static struct wl_list decorations;
static struct wlr_server_decoration_manager *kde_deco_mgr;

struct kde_deco {
	struct wl_list link;  /* decorations */
	struct wlr_server_decoration *wlr_kde_decoration;
	struct view *view;
	struct wl_listener mode;
	struct wl_listener destroy;
};

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct kde_deco *kde_deco = wl_container_of(listener, kde_deco, destroy);
	wl_list_remove(&kde_deco->destroy.link);
	wl_list_remove(&kde_deco->mode.link);
	wl_list_remove(&kde_deco->link);
	free(kde_deco);
}

static void
handle_mode(struct wl_listener *listener, void *data)
{
	struct kde_deco *kde_deco = wl_container_of(listener, kde_deco, mode);
	if (!kde_deco->view) {
		return;
	}

	enum wlr_server_decoration_manager_mode client_mode =
		kde_deco->wlr_kde_decoration->mode;

	switch (client_mode) {
	case WLR_SERVER_DECORATION_MANAGER_MODE_SERVER:
		kde_deco->view->ssd_preference = LAB_SSD_PREF_SERVER;
		break;
	case WLR_SERVER_DECORATION_MANAGER_MODE_NONE:
	case WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT:
		kde_deco->view->ssd_preference = LAB_SSD_PREF_CLIENT;
		break;
	default:
		wlr_log(WLR_ERROR, "Unspecified kde decoration variant "
			"requested: %u", client_mode);
	}

	view_set_decorations(kde_deco->view,
		kde_deco->view->ssd_preference == LAB_SSD_PREF_SERVER);
}

static void
handle_new_server_decoration(struct wl_listener *listener, void *data)
{
	struct wlr_server_decoration *wlr_deco = data;
	struct kde_deco *kde_deco = znew(*kde_deco);
	kde_deco->wlr_kde_decoration = wlr_deco;

	if (wlr_surface_is_xdg_surface(wlr_deco->surface)) {
		/*
		 * Depending on the application event flow, the supplied
		 * wlr_surface may already have been set up as a xdg_surface
		 * or not (e.g. for GTK4). In the second case, the xdg.c
		 * new_surface handler will try to set the view via
		 * kde_server_decoration_set_view().
		 */
		struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_from_wlr_surface(wlr_deco->surface);
		if (xdg_surface && xdg_surface->data) {
			kde_deco->view = (struct view *)xdg_surface->data;
			handle_mode(&kde_deco->mode, wlr_deco);
		}
	}

	wl_signal_add(&wlr_deco->events.destroy, &kde_deco->destroy);
	kde_deco->destroy.notify = handle_destroy;

	wl_signal_add(&wlr_deco->events.mode, &kde_deco->mode);
	kde_deco->mode.notify = handle_mode;

	wl_list_append(&decorations, &kde_deco->link);
}

void
kde_server_decoration_set_view(struct view *view, struct wlr_surface *surface)
{
	struct kde_deco *kde_deco;
	wl_list_for_each(kde_deco, &decorations, link) {
		if (kde_deco->wlr_kde_decoration->surface == surface) {
			if (!kde_deco->view) {
				kde_deco->view = view;
				handle_mode(&kde_deco->mode, kde_deco->wlr_kde_decoration);
			}
			return;
		}
	}
}

void
kde_server_decoration_update_default(void)
{
	assert(kde_deco_mgr);
	wlr_server_decoration_manager_set_default_mode(kde_deco_mgr,
		rc.xdg_shell_server_side_deco
		? WLR_SERVER_DECORATION_MANAGER_MODE_SERVER
		: WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);
}

void
kde_server_decoration_init(struct server *server)
{
	assert(!kde_deco_mgr);
	kde_deco_mgr = wlr_server_decoration_manager_create(server->wl_display);
	if (!kde_deco_mgr) {
		wlr_log(WLR_ERROR, "unable to create the kde server deco manager");
		exit(EXIT_FAILURE);
	}

	wl_list_init(&decorations);
	kde_server_decoration_update_default();

	wl_signal_add(&kde_deco_mgr->events.new_decoration, &server->kde_server_decoration);
	server->kde_server_decoration.notify = handle_new_server_decoration;
}

