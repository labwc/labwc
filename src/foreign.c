// SPDX-License-Identifier: GPL-2.0-only
#include "labwc.h"

static void
handle_toplevel_handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view,
		toplevel_handle_request_minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;
	view_minimize(view, event->minimized);
}

static void
handle_toplevel_handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view,
		toplevel_handle_request_maximize);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;
	view_maximize(view, event->maximized);
}

static void
handle_toplevel_handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view,
		toplevel_handle_request_fullscreen);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
	view_set_fullscreen(view, event->fullscreen, NULL);
}

void
foreign_toplevel_handle_create(struct view *view)
{
	view->toplevel_handle = wlr_foreign_toplevel_handle_v1_create(
		view->server->foreign_toplevel_manager);
	if (!view->toplevel_handle) {
		wlr_log(WLR_ERROR, "cannot create foreign toplevel handle for (%s)",
			view->impl->get_string_prop(view, "title"));
		return;
	}

	view_update_title(view);
	struct wlr_output *wlr_output = view_wlr_output(view);
	if (!wlr_output) {
		wlr_log(WLR_ERROR, "no wlr_output for (%s)",
			view->impl->get_string_prop(view, "title"));
		return;
	}
	wlr_foreign_toplevel_handle_v1_output_enter(view->toplevel_handle,
		wlr_output);

	view->toplevel_handle_request_maximize.notify =
		handle_toplevel_handle_request_maximize;
	wl_signal_add(&view->toplevel_handle->events.request_maximize,
		&view->toplevel_handle_request_maximize);
	view->toplevel_handle_request_minimize.notify =
		handle_toplevel_handle_request_minimize;
	wl_signal_add(&view->toplevel_handle->events.request_minimize,
		&view->toplevel_handle_request_minimize);
	view->toplevel_handle_request_fullscreen.notify =
		handle_toplevel_handle_request_fullscreen;
	wl_signal_add(&view->toplevel_handle->events.request_fullscreen,
		&view->toplevel_handle_request_fullscreen);
	// TODO: hook up remaining signals (close)
}
