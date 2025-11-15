// SPDX-License-Identifier: GPL-2.0-only
#include "input/cursor-context.h"
#include <assert.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include "view.h"

static void
clear_cursor_context(struct cursor_context_saved *saved_ctx)
{
	if (saved_ctx->node_destroy.notify) {
		wl_list_remove(&saved_ctx->node_destroy.link);
	}
	if (saved_ctx->surface_destroy.notify) {
		wl_list_remove(&saved_ctx->surface_destroy.link);
	}
	if (saved_ctx->view_destroy.notify) {
		wl_list_remove(&saved_ctx->view_destroy.link);
	}
	*saved_ctx = (struct cursor_context_saved) {0};
}

static void
handle_node_destroy(struct wl_listener *listener, void *data)
{
	struct cursor_context_saved *saved_ctx =
		wl_container_of(listener, saved_ctx, node_destroy);
	clear_cursor_context(saved_ctx);
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct cursor_context_saved *saved_ctx =
		wl_container_of(listener, saved_ctx, surface_destroy);
	clear_cursor_context(saved_ctx);
}

static void
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct cursor_context_saved *saved_ctx =
		wl_container_of(listener, saved_ctx, view_destroy);
	clear_cursor_context(saved_ctx);
}

void
cursor_context_save(struct cursor_context_saved *saved_ctx,
		const struct cursor_context *ctx)
{
	clear_cursor_context(saved_ctx);
	if (!ctx) {
		return;
	}
	saved_ctx->ctx = *ctx;
	if (ctx->node) {
		saved_ctx->node_destroy.notify = handle_node_destroy;
		wl_signal_add(&ctx->node->events.destroy, &saved_ctx->node_destroy);
	}
	if (ctx->surface) {
		saved_ctx->surface_destroy.notify = handle_surface_destroy;
		wl_signal_add(&ctx->surface->events.destroy, &saved_ctx->surface_destroy);
	}
	if (ctx->view) {
		saved_ctx->view_destroy.notify = handle_view_destroy;
		wl_signal_add(&ctx->view->events.destroy, &saved_ctx->view_destroy);
	}
}
