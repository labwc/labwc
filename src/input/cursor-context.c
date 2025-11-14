// SPDX-License-Identifier: GPL-2.0-only
#include "input/cursor-context.h"
#include <assert.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include "common/scene-helpers.h"
#include "dnd.h"
#include "labwc.h"
#include "node.h"
#include "ssd.h"
#include "view.h"

/*
 * Work around rounding issues in some clients (notably Qt apps) where
 * cursor coordinates in the rightmost or bottom pixel are incorrectly
 * rounded up, putting them outside the surface bounds. The effect is
 * especially noticeable in right/bottom desktop panels, since driving
 * the cursor to the edge of the screen no longer works.
 *
 * Under X11, such rounding issues went unnoticed since cursor positions
 * were always integers (i.e. whole pixel boundaries) anyway. Until more
 * clients/toolkits are fractional-pixel clean, limit surface cursor
 * coordinates to (w - 1, h - 1) as a workaround.
 */
static void
avoid_edge_rounding_issues(struct cursor_context *ctx)
{
	if (!ctx->surface) {
		return;
	}

	int w = ctx->surface->current.width;
	int h = ctx->surface->current.height;
	/*
	 * The cursor isn't expected to be outside the surface bounds
	 * here, but check (sx < w, sy < h) just in case.
	 */
	if (ctx->sx > w - 1 && ctx->sx < w) {
		ctx->sx = w - 1;
	}
	if (ctx->sy > h - 1 && ctx->sy < h) {
		ctx->sy = h - 1;
	}
}

/* TODO: make this less big and scary */
struct cursor_context
get_cursor_context(struct server *server)
{
	struct cursor_context ret = {.type = LAB_NODE_NONE};
	struct wlr_cursor *cursor = server->seat.cursor;

	/* Prevent drag icons to be on top of the hitbox detection */
	if (server->seat.drag.active) {
		dnd_icons_show(&server->seat, false);
	}

	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->scene->tree.node,
			cursor->x, cursor->y, &ret.sx, &ret.sy);

	if (server->seat.drag.active) {
		dnd_icons_show(&server->seat, true);
	}

	if (!node) {
		ret.type = LAB_NODE_ROOT;
		return ret;
	}
	ret.node = node;
	ret.surface = lab_wlr_surface_from_node(node);

	avoid_edge_rounding_issues(&ret);

#if HAVE_XWAYLAND
	/* TODO: attach LAB_NODE_UNMANAGED node-descriptor to unmanaged surfaces */
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		if (node->parent == server->unmanaged_tree) {
			ret.type = LAB_NODE_UNMANAGED;
			return ret;
		}
	}
#endif
	while (node) {
		struct node_descriptor *desc = node->data;
		if (desc) {
			switch (desc->type) {
			case LAB_NODE_VIEW:
			case LAB_NODE_XDG_POPUP:
				ret.view = desc->view;
				if (ret.surface) {
					ret.type = LAB_NODE_CLIENT;
				} else {
					/* e.g. when cursor is on resize-indicator */
					ret.type = LAB_NODE_NONE;
				}
				return ret;
			case LAB_NODE_LAYER_SURFACE:
				ret.type = LAB_NODE_LAYER_SURFACE;
				return ret;
			case LAB_NODE_LAYER_POPUP:
			case LAB_NODE_SESSION_LOCK_SURFACE:
			case LAB_NODE_IME_POPUP:
				ret.type = LAB_NODE_CLIENT;
				return ret;
			case LAB_NODE_MENUITEM:
				/* Always return the top scene node for menu items */
				ret.node = node;
				ret.type = LAB_NODE_MENUITEM;
				return ret;
			case LAB_NODE_OSD_ITEM:
				/* Always return the top scene node for osd items */
				ret.node = node;
				ret.type = LAB_NODE_OSD_ITEM;
				return ret;
			case LAB_NODE_BUTTON_FIRST...LAB_NODE_BUTTON_LAST:
			case LAB_NODE_SSD_ROOT:
			case LAB_NODE_TITLE:
			case LAB_NODE_TITLEBAR:
				/* Always return the top scene node for ssd parts */
				ret.node = node;
				ret.view = desc->view;
				/*
				 * A node_descriptor attached to a ssd part
				 * must have an associated view.
				 */
				assert(ret.view);

				/*
				 * When cursor is on the ssd border or extents,
				 * desc->type is usually LAB_NODE_SSD_ROOT.
				 * But desc->type can also be LAB_NODE_TITLEBAR
				 * when cursor is on the curved border at the
				 * titlebar.
				 *
				 * ssd_get_resizing_type() overwrites both of
				 * them with LAB_NODE_{BORDER,CORNER}_* node
				 * types, which are mapped to mouse contexts
				 * like Left and TLCorner.
				 */
				ret.type = ssd_get_resizing_type(ret.view->ssd, cursor);
				if (ret.type == LAB_NODE_NONE) {
					/*
					 * If cursor is not on border/extents,
					 * just use desc->type which should be
					 * mapped to mouse contexts like Title,
					 * Titlebar and Iconify.
					 */
					ret.type = desc->type;
				}

				return ret;
			default:
				/* Other node types are not attached a scene node */
				wlr_log(WLR_ERROR, "unexpected node type: %d", desc->type);
				break;
			}
		}

		/* node->parent is always a *wlr_scene_tree */
		node = node->parent ? &node->parent->node : NULL;
	}

	/*
	 * TODO: add node descriptors for the OSDs and reinstate
	 *       wlr_log(WLR_DEBUG, "Unknown node detected");
	 */
	return ret;
}

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
