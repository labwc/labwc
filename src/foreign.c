// SPDX-License-Identifier: GPL-2.0-only
#include "labwc.h"
#include "view.h"
#include "workspaces.h"

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, toplevel.minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;
	view_minimize(view, event->minimized);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, toplevel.maximize);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;
	view_maximize(view, event->maximized, /*store_natural_geometry*/ true);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, toplevel.fullscreen);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
	view_set_fullscreen(view, event->fullscreen, NULL);
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, toplevel.activate);
	// struct wlr_foreign_toplevel_handle_v1_activated_event *event = data;
	/* In a multi-seat world we would select seat based on event->seat here. */
	if (view->workspace != view->server->workspace_current) {
		workspaces_switch_to(view->workspace);
	}
	desktop_focus_and_activate_view(&view->server->seat, view);
	desktop_move_to_front(view);
}

static void
handle_request_close(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, toplevel.close);
	view_close(view);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, toplevel.destroy);
	struct foreign_toplevel *toplevel = &view->toplevel;
	wl_list_remove(&toplevel->maximize.link);
	wl_list_remove(&toplevel->minimize.link);
	wl_list_remove(&toplevel->fullscreen.link);
	wl_list_remove(&toplevel->activate.link);
	wl_list_remove(&toplevel->close.link);
	wl_list_remove(&toplevel->destroy.link);
	toplevel->handle = NULL;
}

void
foreign_toplevel_handle_create(struct view *view)
{
	struct foreign_toplevel *toplevel = &view->toplevel;

	toplevel->handle = wlr_foreign_toplevel_handle_v1_create(
		view->server->foreign_toplevel_manager);
	if (!toplevel->handle) {
		wlr_log(WLR_ERROR, "cannot create foreign toplevel handle for (%s)",
			view_get_string_prop(view, "title"));
		return;
	}

	struct wlr_output *wlr_output = view_wlr_output(view);
	if (!wlr_output) {
		wlr_log(WLR_ERROR, "no wlr_output for (%s)",
			view_get_string_prop(view, "title"));
		return;
	}
	wlr_foreign_toplevel_handle_v1_output_enter(toplevel->handle, wlr_output);

	toplevel->maximize.notify = handle_request_maximize;
	wl_signal_add(&toplevel->handle->events.request_maximize,
		&toplevel->maximize);

	toplevel->minimize.notify = handle_request_minimize;
	wl_signal_add(&toplevel->handle->events.request_minimize,
		&toplevel->minimize);

	toplevel->fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&toplevel->handle->events.request_fullscreen,
		&toplevel->fullscreen);

	toplevel->activate.notify = handle_request_activate;
	wl_signal_add(&toplevel->handle->events.request_activate,
		&toplevel->activate);

	toplevel->close.notify = handle_request_close;
	wl_signal_add(&toplevel->handle->events.request_close,
		&toplevel->close);

	toplevel->destroy.notify = handle_destroy;
	wl_signal_add(&toplevel->handle->events.destroy, &toplevel->destroy);
}
