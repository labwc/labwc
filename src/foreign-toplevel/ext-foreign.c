// SPDX-License-Identifier: GPL-2.0-only
#include "foreign-toplevel/ext-foreign.h"
#include <assert.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include "common/macros.h"
#include "labwc.h"
#include "view.h"

/* ext signals */
static void
handle_handle_destroy(struct wl_listener *listener, void *data)
{
	struct ext_foreign_toplevel *ext_toplevel =
		wl_container_of(listener, ext_toplevel, on.handle_destroy);

	/* Client side requests */
	wl_list_remove(&ext_toplevel->on.handle_destroy.link);

	/* Compositor side state changes */
	wl_list_remove(&ext_toplevel->on_view.new_app_id.link);
	wl_list_remove(&ext_toplevel->on_view.new_title.link);

	ext_toplevel->handle = NULL;
}

/* Compositor signals */
static void
handle_new_app_id(struct wl_listener *listener, void *data)
{
	struct ext_foreign_toplevel *ext_toplevel =
		wl_container_of(listener, ext_toplevel, on_view.new_app_id);
	assert(ext_toplevel->handle);

	struct wlr_ext_foreign_toplevel_handle_v1_state state = {
		.title = ext_toplevel->view->title,
		.app_id = ext_toplevel->view->app_id,
	};
	wlr_ext_foreign_toplevel_handle_v1_update_state(ext_toplevel->handle,
		&state);
}

static void
handle_new_title(struct wl_listener *listener, void *data)
{
	struct ext_foreign_toplevel *ext_toplevel =
		wl_container_of(listener, ext_toplevel, on_view.new_title);
	assert(ext_toplevel->handle);

	struct wlr_ext_foreign_toplevel_handle_v1_state state = {
		.title = ext_toplevel->view->title,
		.app_id = ext_toplevel->view->app_id,
	};
	wlr_ext_foreign_toplevel_handle_v1_update_state(ext_toplevel->handle,
		&state);
}

/* Internal API */
void
ext_foreign_toplevel_init(struct ext_foreign_toplevel *ext_toplevel,
		struct view *view)
{
	assert(view->server->foreign_toplevel_list);
	ext_toplevel->view = view;

	struct wlr_ext_foreign_toplevel_handle_v1_state state = {
		.title = view->title,
		.app_id = view->app_id,
	};
	ext_toplevel->handle = wlr_ext_foreign_toplevel_handle_v1_create(
		view->server->foreign_toplevel_list, &state);

	if (!ext_toplevel->handle) {
		wlr_log(WLR_ERROR, "cannot create ext toplevel handle for (%s)",
			view->title);
		return;
	}

	/* Client side requests */
	ext_toplevel->on.handle_destroy.notify = handle_handle_destroy;
	wl_signal_add(&ext_toplevel->handle->events.destroy, &ext_toplevel->on.handle_destroy);

	/* Compositor side state changes */
	CONNECT_SIGNAL(view, &ext_toplevel->on_view, new_app_id);
	CONNECT_SIGNAL(view, &ext_toplevel->on_view, new_title);
}

void
ext_foreign_toplevel_finish(struct ext_foreign_toplevel *ext_toplevel)
{
	if (!ext_toplevel->handle) {
		return;
	}

	/* invokes handle_handle_destroy() which does more cleanup */
	wlr_ext_foreign_toplevel_handle_v1_destroy(ext_toplevel->handle);
	assert(!ext_toplevel->handle);
}
