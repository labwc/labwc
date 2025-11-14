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

/**
 * get_cursor_context - find view, surface and scene_node at cursor
 *
 * If the cursor is on a client-drawn surface:
 * - ctx.{surface,node} points to the surface, which may be a subsurface.
 * - ctx.view is set if the node is associated to a xdg/x11 window.
 * - ctx.type is LAYER_SURFACE or UNMANAGED if the node is a layer-shell
 *   surface or an X11 unmanaged surface. Otherwise, CLIENT is set.
 *
 * If the cursor is on a server-side component (SSD part and menu item):
 * - ctx.node points to the root node of that component
 * - ctx.view is set if the component is a SSD part
 * - ctx.type specifies the component (e.g. MENU_ITEM, BORDER_TOP, BUTTON_ICONIFY)
 *
 * If no node is found at cursor, ctx.type is set to ROOT.
 */
struct cursor_context get_cursor_context(struct server *server);

/*
 * Save ctx to saved_ctx. saved_ctx is cleared when either
 * of its node, surface and view is destroyed.
 */
void cursor_context_save(struct cursor_context_saved *saved_ctx,
	const struct cursor_context *ctx);

#endif /* LABWC_CURSOR_CONTEXT_H */
