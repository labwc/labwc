// SPDX-License-Identifier: GPL-2.0-only
#include "foreign-toplevel/wlr-foreign.h"
#include <assert.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include "common/macros.h"
#include "labwc.h"
#include "output.h"
#include "view.h"

/* wlr signals */
static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on.request_minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;

	view_minimize(wlr_toplevel->view, event->minimized);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on.request_maximize);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;

	view_maximize(wlr_toplevel->view,
		event->maximized ? VIEW_AXIS_BOTH : VIEW_AXIS_NONE,
		/*store_natural_geometry*/ true);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on.request_fullscreen);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;

	/* TODO: This ignores event->output */
	view_set_fullscreen(wlr_toplevel->view, event->fullscreen);
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on.request_activate);

	/* In a multi-seat world we would select seat based on event->seat here. */
	desktop_focus_view(wlr_toplevel->view, /*raise*/ true);
}

static void
handle_request_close(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on.request_close);

	view_close(wlr_toplevel->view);
}

static void
handle_handle_destroy(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on.handle_destroy);

	/* Client side requests */
	wl_list_remove(&wlr_toplevel->on.request_maximize.link);
	wl_list_remove(&wlr_toplevel->on.request_minimize.link);
	wl_list_remove(&wlr_toplevel->on.request_fullscreen.link);
	wl_list_remove(&wlr_toplevel->on.request_activate.link);
	wl_list_remove(&wlr_toplevel->on.request_close.link);
	wl_list_remove(&wlr_toplevel->on.handle_destroy.link);

	/* Compositor side state changes */
	wl_list_remove(&wlr_toplevel->on_view.new_app_id.link);
	wl_list_remove(&wlr_toplevel->on_view.new_title.link);
	wl_list_remove(&wlr_toplevel->on_view.new_outputs.link);
	wl_list_remove(&wlr_toplevel->on_view.maximized.link);
	wl_list_remove(&wlr_toplevel->on_view.minimized.link);
	wl_list_remove(&wlr_toplevel->on_view.fullscreened.link);
	wl_list_remove(&wlr_toplevel->on_view.activated.link);

	wlr_toplevel->handle = NULL;
}

/* Compositor signals */
static void
handle_new_app_id(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on_view.new_app_id);
	assert(wlr_toplevel->handle);

	wlr_foreign_toplevel_handle_v1_set_app_id(wlr_toplevel->handle,
		wlr_toplevel->view->app_id);
}

static void
handle_new_title(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on_view.new_title);
	assert(wlr_toplevel->handle);

	wlr_foreign_toplevel_handle_v1_set_title(wlr_toplevel->handle,
		wlr_toplevel->view->title);
}

static void
handle_new_outputs(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on_view.new_outputs);
	assert(wlr_toplevel->handle);

	/*
	 * Loop over all outputs and notify foreign_toplevel clients about changes.
	 * wlr_foreign_toplevel_handle_v1_output_xxx() keeps track of the active
	 * outputs internally and merges the events. It also listens to output
	 * destroy events so its fine to just relay the current state and let
	 * wlr_foreign_toplevel handle the rest.
	 */
	struct output *output;
	wl_list_for_each(output, &wlr_toplevel->view->server->outputs, link) {
		if (view_on_output(wlr_toplevel->view, output)) {
			wlr_foreign_toplevel_handle_v1_output_enter(
				wlr_toplevel->handle, output->wlr_output);
		} else {
			wlr_foreign_toplevel_handle_v1_output_leave(
				wlr_toplevel->handle, output->wlr_output);
		}
	}
}

static void
handle_maximized(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on_view.maximized);
	assert(wlr_toplevel->handle);

	wlr_foreign_toplevel_handle_v1_set_maximized(wlr_toplevel->handle,
		wlr_toplevel->view->maximized == VIEW_AXIS_BOTH);
}

static void
handle_minimized(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on_view.minimized);
	assert(wlr_toplevel->handle);

	wlr_foreign_toplevel_handle_v1_set_minimized(wlr_toplevel->handle,
		wlr_toplevel->view->minimized);
}

static void
handle_fullscreened(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on_view.fullscreened);
	assert(wlr_toplevel->handle);

	wlr_foreign_toplevel_handle_v1_set_fullscreen(wlr_toplevel->handle,
		wlr_toplevel->view->fullscreen);
}

static void
handle_activated(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel =
		wl_container_of(listener, wlr_toplevel, on_view.activated);
	assert(wlr_toplevel->handle);

	bool *activated = data;
	wlr_foreign_toplevel_handle_v1_set_activated(wlr_toplevel->handle,
		*activated);
}

/* Internal API */
void
wlr_foreign_toplevel_init(struct wlr_foreign_toplevel *wlr_toplevel,
		struct view *view)
{
	assert(view->server->foreign_toplevel_manager);
	wlr_toplevel->view = view;

	wlr_toplevel->handle = wlr_foreign_toplevel_handle_v1_create(
		view->server->foreign_toplevel_manager);
	if (!wlr_toplevel->handle) {
		wlr_log(WLR_ERROR, "cannot create wlr foreign toplevel handle for (%s)",
			view->title);
		return;
	}

	/* These states may be set before the initial map */
	handle_new_app_id(&wlr_toplevel->on_view.new_app_id, NULL);
	handle_new_title(&wlr_toplevel->on_view.new_title, NULL);
	handle_new_outputs(&wlr_toplevel->on_view.new_outputs, NULL);
	handle_maximized(&wlr_toplevel->on_view.maximized, NULL);
	handle_minimized(&wlr_toplevel->on_view.minimized, NULL);
	handle_fullscreened(&wlr_toplevel->on_view.fullscreened, NULL);
	handle_activated(&wlr_toplevel->on_view.activated,
		&(bool){view == view->server->active_view});

	/* Client side requests */
	CONNECT_SIGNAL(wlr_toplevel->handle, &wlr_toplevel->on, request_maximize);
	CONNECT_SIGNAL(wlr_toplevel->handle, &wlr_toplevel->on, request_minimize);
	CONNECT_SIGNAL(wlr_toplevel->handle, &wlr_toplevel->on, request_fullscreen);
	CONNECT_SIGNAL(wlr_toplevel->handle, &wlr_toplevel->on, request_activate);
	CONNECT_SIGNAL(wlr_toplevel->handle, &wlr_toplevel->on, request_close);
	wlr_toplevel->on.handle_destroy.notify = handle_handle_destroy;
	wl_signal_add(&wlr_toplevel->handle->events.destroy, &wlr_toplevel->on.handle_destroy);

	/* Compositor side state changes */
	CONNECT_SIGNAL(view, &wlr_toplevel->on_view, new_app_id);
	CONNECT_SIGNAL(view, &wlr_toplevel->on_view, new_title);
	CONNECT_SIGNAL(view, &wlr_toplevel->on_view, new_outputs);
	CONNECT_SIGNAL(view, &wlr_toplevel->on_view, maximized);
	CONNECT_SIGNAL(view, &wlr_toplevel->on_view, minimized);
	CONNECT_SIGNAL(view, &wlr_toplevel->on_view, fullscreened);
	CONNECT_SIGNAL(view, &wlr_toplevel->on_view, activated);
}

void
wlr_foreign_toplevel_set_parent(struct wlr_foreign_toplevel *wlr_toplevel,
		struct wlr_foreign_toplevel *parent)
{
	if (!wlr_toplevel->handle) {
		return;
	}

	/* The wlroots wlr-foreign-toplevel impl ensures parent is reset to NULL on destroy */
	wlr_foreign_toplevel_handle_v1_set_parent(wlr_toplevel->handle,
		parent ? parent->handle : NULL);
}

void
wlr_foreign_toplevel_finish(struct wlr_foreign_toplevel *wlr_toplevel)
{
	if (!wlr_toplevel->handle) {
		return;
	}

	/* invokes handle_handle_destroy() which does more cleanup */
	wlr_foreign_toplevel_handle_v1_destroy(wlr_toplevel->handle);
	assert(!wlr_toplevel->handle);
}
