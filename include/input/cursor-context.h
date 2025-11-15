/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_CURSOR_CONTEXT_H
#define LABWC_CURSOR_CONTEXT_H

#include <wayland-server-core.h>
#include "common/node-type.h"

struct server;

struct cursor_context {
	struct view *view;
	struct wlr_scene_node *node;
	struct wlr_surface *surface;
	enum lab_node_type type;
	double sx, sy;
};

/* Used to persistently store cursor_context (e.g. in seat->pressed) */
struct cursor_context_saved {
	struct cursor_context ctx;
	struct wl_listener view_destroy;
	struct wl_listener node_destroy;
	struct wl_listener surface_destroy;
};

/*
 * Save ctx to saved_ctx. saved_ctx is cleared when either
 * of its node, surface and view is destroyed.
 */
void cursor_context_save(struct cursor_context_saved *saved_ctx,
	const struct cursor_context *ctx);

#endif /* LABWC_CURSOR_CONTEXT_H */
