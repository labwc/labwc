// SPDX-License-Identifier: GPL-2.0-only
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include "common/graphic-helpers.h"
#include "common/scene-helpers.h"
#include "debug.h"
#include "labwc.h"
#include "node.h"
#include "ssd.h"
#include "view.h"
#include "workspaces.h"

#define HEADER_CHARS "------------------------------"

#define INDENT_SIZE 3
#define LEFT_COL_SPACE 35

#define IGNORE_SSD true
#define IGNORE_MENU true
#define IGNORE_OSD_PREVIEW_OUTLINE true
#define IGNORE_SNAPPING_PREVIEW_OUTLINE true

static struct view *last_view;

static const char *
get_node_type(struct wlr_scene_node *node)
{
	switch (node->type) {
	case WLR_SCENE_NODE_TREE:
		if (!node->parent) {
			return "root";
		}
		return "tree";
	case WLR_SCENE_NODE_RECT:
		return "rect";
	case WLR_SCENE_NODE_BUFFER:
		if (lab_wlr_surface_from_node(node)) {
			return "surface";
		}
		return "buffer";
	}
	return "error";
}

static const char *
get_layer_name(uint32_t layer)
{
	switch (layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return "output->layer-background";
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return "output->layer-bottom";
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return "output->layer-top";
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return "output->layer-overlay";
	default:
		abort();
	}
}

static const char *
get_view_part(struct view *view, struct wlr_scene_node *node)
{
	static char view_name[LEFT_COL_SPACE];
	if (!view) {
		return NULL;
	}
	if (node == &view->scene_tree->node) {
		const char *app_id = view_get_string_prop(view, "app_id");
		if (!app_id) {
			return "view";
		}
		snprintf(view_name, sizeof(view_name), "view (%s)", app_id);
		return view_name;
	}
	if (node == view->scene_node) {
		return "view->scene_node";
	}
	if (view->resize_indicator.tree
			&& node == &view->resize_indicator.tree->node) {
		/* Created on-demand */
		return "view->resize_indicator";
	}
	return ssd_debug_get_node_name(view->ssd, node);
}

static const char *
get_special(struct server *server, struct wlr_scene_node *node)
{
	if (node == &server->scene->tree.node) {
		return "server->scene";
	}
	if (node == &server->menu_tree->node) {
		return "server->menu_tree";
	}
	if (node == &server->view_tree->node) {
		return "server->view_tree";
	}
	if (node == &server->view_tree_always_on_bottom->node) {
		return "server->always_on_bottom";
	}
	if (node == &server->view_tree_always_on_top->node) {
		return "server->always_on_top";
	}
	if (node->parent == server->view_tree) {
		struct workspace *workspace;
		wl_list_for_each(workspace, &server->workspaces, link) {
			if (&workspace->tree->node == node) {
				return workspace->name;
			}
		}
		return "unknown workspace";
	}
	if (node->parent == &server->scene->tree) {
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
			if (node == &output->osd_tree->node) {
				return "output->osd_tree";
			}
			if (node == &output->layer_popup_tree->node) {
				return "output->layer_popup_tree";
			}
			for (int i = 0; i < 4; i++) {
				if (node == &output->layer_tree[i]->node) {
					return get_layer_name(i);
				}
			}
			if (node == &output->session_lock_tree->node) {
				return "output->session_lock_tree";
			}
		}
	}
	if (node == &server->xdg_popup_tree->node) {
		return "server->xdg_popup_tree";
	}
	if (node == &server->seat.drag.icons->node) {
		return "seat->drag.icons";
	}
	if (server->seat.overlay.region_rect.node
			&& node == server->seat.overlay.region_rect.node) {
		/* Created on-demand */
		return "seat->overlay.region_rect";
	}
	if (server->seat.overlay.edge_rect.node
			&& node == server->seat.overlay.edge_rect.node) {
		/* Created on-demand */
		return "seat->overlay.edge_rect";
	}
	if (server->seat.input_method_relay->popup_tree
			&& node == &server->seat.input_method_relay->popup_tree->node) {
		/* Created on-demand */
		return "seat->im_relay->popup_tree";
	}
	if (server->osd_state.preview_outline
			&& node == &server->osd_state.preview_outline->tree->node) {
		/* Created on-demand */
		return "osd_state->preview_outline";
	}
#if HAVE_XWAYLAND
	if (node == &server->unmanaged_tree->node) {
		return "server->unmanaged_tree";
	}
#endif
	struct wlr_scene_tree *grand_parent =
		node->parent ? node->parent->node.parent : NULL;
	if (grand_parent == server->view_tree && node->data) {
		last_view = node_view_from_node(node);
	}
	if (node->parent == server->view_tree_always_on_top && node->data) {
		last_view = node_view_from_node(node);
	}
	const char *view_part = get_view_part(last_view, node);
	if (view_part) {
		return view_part;
	}
	return get_node_type(node);
}

struct pad {
	uint8_t left;
	uint8_t right;
};

static struct pad
get_center_padding(const char *text, uint8_t max_width)
{
	struct pad pad;
	size_t text_len = strlen(text);
	pad.left = (double)(max_width - text_len) / 2 + 0.5f;
	pad.right = max_width - pad.left - text_len;
	return pad;
}

static void
dump_tree(struct server *server, struct wlr_scene_node *node,
	int pos, int x, int y)
{
	const char *type = get_special(server, node);

	if (pos) {
		printf("%*c+-- ", pos, ' ');
	} else {
		struct pad node_pad = get_center_padding("Node", 16);
		printf(" %*c %4s  %4s  %*c%s\n", LEFT_COL_SPACE + 4, ' ',
			"X", "Y", node_pad.left, ' ', "Node");
		printf(" %*c %.4s  %.4s  %.16s\n", LEFT_COL_SPACE + 4, ' ',
			HEADER_CHARS, HEADER_CHARS, HEADER_CHARS);
		printf(" ");
	}
	int max_width = LEFT_COL_SPACE - pos;
	int padding = max_width - strlen(type);
	if (padding < 0) {
		padding = 0;
	}
	if (!pos) {
		padding += 3;
	}
	printf("%.*s %*c %4d  %4d  [%p]\n", max_width - 1, type, padding, ' ', x, y, node);

	if ((IGNORE_MENU && node == &server->menu_tree->node)
			|| (IGNORE_SSD && last_view
				&& ssd_debug_is_root_node(last_view->ssd, node))
			|| (IGNORE_OSD_PREVIEW_OUTLINE && server->osd_state.preview_outline
				&& node == &server->osd_state.preview_outline->tree->node)
			|| (IGNORE_SNAPPING_PREVIEW_OUTLINE && server->seat.overlay.region_rect.node
				&& !server->seat.overlay.region_rect.fill
				&& node == server->seat.overlay.region_rect.node)
			|| (IGNORE_SNAPPING_PREVIEW_OUTLINE && server->seat.overlay.edge_rect.node
				&& !server->seat.overlay.edge_rect.fill
				&& node == server->seat.overlay.edge_rect.node)) {
		printf("%*c%s\n", pos + 4 + INDENT_SIZE, ' ', "<skipping children>");
		return;
	}

	if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_node *child;
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		wl_list_for_each(child, &tree->children, link) {
			dump_tree(server, child, pos + INDENT_SIZE,
				x + child->x, y + child->y);
		}
	}
}

void
debug_dump_scene(struct server *server)
{
	printf("\n");
	dump_tree(server, &server->scene->tree.node, 0, 0, 0);
	printf("\n");

	/*
	 * Reset last_view so we don't access a
	 * potentially free'd pointer on the next call
	 */
	last_view = NULL;
}
