// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include "common/macros.h"
#include "labwc.h"
#include "view.h"
#include "foreign-toplevel-internal.h"

/* wlr signals */
static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel = wl_container_of(
		listener, toplevel, wlr_toplevel.on.request_minimize);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;

	foreign_request_minimize(toplevel, event->minimized);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel = wl_container_of(
		listener, toplevel, wlr_toplevel.on.request_maximize);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;

	foreign_request_maximize(toplevel,
		event->maximized ? VIEW_AXIS_BOTH : VIEW_AXIS_NONE);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel = wl_container_of(
		listener, toplevel, wlr_toplevel.on.request_fullscreen);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;

	/* TODO: This ignores event->output */
	foreign_request_fullscreen(toplevel, event->fullscreen);
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel = wl_container_of(
		listener, toplevel, wlr_toplevel.on.request_activate);

	/* In a multi-seat world we would select seat based on event->seat here. */
	foreign_request_activate(toplevel);
}

static void
handle_request_close(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel = wl_container_of(
		listener, toplevel, wlr_toplevel.on.request_close);

	foreign_request_close(toplevel);
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
	wlr_toplevel->handle = NULL;
}

/* Compositor signals */
static void
handle_new_app_id(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, wlr_toplevel.on_view.new_app_id);
	assert(toplevel->wlr_toplevel.handle);

	const char *app_id = view_get_string_prop(toplevel->view, "app_id");
	wlr_foreign_toplevel_handle_v1_set_app_id(toplevel->wlr_toplevel.handle, app_id);
}

static void
handle_new_title(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, wlr_toplevel.on_view.new_title);
	assert(toplevel->wlr_toplevel.handle);

	const char *title = view_get_string_prop(toplevel->view, "title");
	wlr_foreign_toplevel_handle_v1_set_title(toplevel->wlr_toplevel.handle, title);
}

static void
handle_new_outputs(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, wlr_toplevel.on_view.new_outputs);
	assert(toplevel->wlr_toplevel.handle);

	/*
	 * Loop over all outputs and notify foreign_toplevel clients about changes.
	 * wlr_foreign_toplevel_handle_v1_output_xxx() keeps track of the active
	 * outputs internally and merges the events. It also listens to output
	 * destroy events so its fine to just relay the current state and let
	 * wlr_foreign_toplevel handle the rest.
	 */
	struct output *output;
	wl_list_for_each(output, &toplevel->view->server->outputs, link) {
		if (view_on_output(toplevel->view, output)) {
			wlr_foreign_toplevel_handle_v1_output_enter(
				toplevel->wlr_toplevel.handle, output->wlr_output);
		} else {
			wlr_foreign_toplevel_handle_v1_output_leave(
				toplevel->wlr_toplevel.handle, output->wlr_output);
		}
	}
}

static void
handle_maximized(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, wlr_toplevel.on_view.maximized);
	assert(toplevel->wlr_toplevel.handle);

	wlr_foreign_toplevel_handle_v1_set_maximized(
		toplevel->wlr_toplevel.handle,
		toplevel->view->maximized == VIEW_AXIS_BOTH);
}

static void
handle_minimized(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, wlr_toplevel.on_view.minimized);
	assert(toplevel->wlr_toplevel.handle);

	wlr_foreign_toplevel_handle_v1_set_minimized(
		toplevel->wlr_toplevel.handle, toplevel->view->minimized);
}

static void
handle_fullscreened(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, wlr_toplevel.on_view.fullscreened);
	assert(toplevel->wlr_toplevel.handle);

	wlr_foreign_toplevel_handle_v1_set_fullscreen(
		toplevel->wlr_toplevel.handle, toplevel->view->fullscreen);
}

static void
handle_activated(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, wlr_toplevel.on_view.activated);
	assert(toplevel->wlr_toplevel.handle);

	bool *activated = data;
	wlr_foreign_toplevel_handle_v1_set_activated(
		toplevel->wlr_toplevel.handle, *activated);
}

/* Internal signals */
static void
handle_toplevel_parent(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel = wl_container_of(
		listener, wlr_toplevel, on_foreign_toplevel.toplevel_parent);
	struct foreign_toplevel *parent = data;

	assert(wlr_toplevel->handle);

	/* The wlroots wlr-foreign-toplevel impl ensures parent is reset to NULL on destroy */
	wlr_foreign_toplevel_handle_v1_set_parent(wlr_toplevel->handle, parent
		? parent->wlr_toplevel.handle
		: NULL);
}

static void
handle_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct wlr_foreign_toplevel *wlr_toplevel = wl_container_of(
		listener, wlr_toplevel, on_foreign_toplevel.toplevel_destroy);

	assert(wlr_toplevel->handle);
	wlr_foreign_toplevel_handle_v1_destroy(wlr_toplevel->handle);

	/* Compositor side state changes */
	wl_list_remove(&wlr_toplevel->on_view.new_app_id.link);
	wl_list_remove(&wlr_toplevel->on_view.new_title.link);
	wl_list_remove(&wlr_toplevel->on_view.new_outputs.link);
	wl_list_remove(&wlr_toplevel->on_view.maximized.link);
	wl_list_remove(&wlr_toplevel->on_view.minimized.link);
	wl_list_remove(&wlr_toplevel->on_view.fullscreened.link);
	wl_list_remove(&wlr_toplevel->on_view.activated.link);

	/* Internal signals */
	wl_list_remove(&wlr_toplevel->on_foreign_toplevel.toplevel_parent.link);
	wl_list_remove(&wlr_toplevel->on_foreign_toplevel.toplevel_destroy.link);
}

/* Internal API */
void
wlr_foreign_toplevel_init(struct foreign_toplevel *toplevel)
{
	struct wlr_foreign_toplevel *wlr_toplevel = &toplevel->wlr_toplevel;
	struct view *view = toplevel->view;

	assert(view->server->foreign_toplevel_manager);

	wlr_toplevel->handle = wlr_foreign_toplevel_handle_v1_create(
		view->server->foreign_toplevel_manager);
	if (!wlr_toplevel->handle) {
		wlr_log(WLR_ERROR, "cannot create wlr foreign toplevel handle for (%s)",
			view_get_string_prop(view, "title"));
		return;
	}

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

	/* Internal signals */
	CONNECT_SIGNAL(toplevel, &wlr_toplevel->on_foreign_toplevel, toplevel_parent);
	CONNECT_SIGNAL(toplevel, &wlr_toplevel->on_foreign_toplevel, toplevel_destroy);
}
