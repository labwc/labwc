// SPDX-License-Identifier: GPL-2.0-only
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include "common/scene-helpers.h"
#include "labwc.h"
#include "node.h"
#include "ssd.h"
#include "view.h"

#define HEADER_CHARS "------------------------------"

#define INDENT_SIZE 3
#define IGNORE_SSD true
#define IGNORE_MENU true
#define LEFT_COL_SPACE 35

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
		return "layer-background";
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return "layer-bottom";
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return "layer-top";
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return "layer-overlay";
	default:
		abort();
	}
}

static const char *
get_view_part(struct view *view, struct wlr_scene_node *node)
{
	if (view && node == &view->scene_tree->node) {
		return "view";
	}
	if (view && node == view->scene_node) {
		return "view->scene_node";
	}
	if (view) {
		return ssd_debug_get_node_name(view->ssd, node);
	}
	return NULL;
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
	if (node == &server->view_tree_always_on_top->node) {
		return "server->view_tree_always_on_top";
	}
	if (node->parent == server->view_tree) {
		/* Add node_descriptor just to get the name here? */
		return "workspace";
	}
	if (node->parent == &server->scene->tree) {
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
			if (node == &output->osd_tree->node) {
				return "output->osd_tree";
			}
			if (node == &output->layer_popup_tree->node) {
				return "output->popup_tree";
			}
			for (int i = 0; i < 4; i++) {
				if (node == &output->layer_tree[i]->node) {
					return get_layer_name(i);
				}
			}
		}
	}
#if HAVE_XWAYLAND
	if (node == &server->unmanaged_tree->node) {
		return "server->unmanaged_tree";
	}
#endif
	struct wlr_scene_tree *grand_parent =
		node->parent ? node->parent->node.parent : NULL;
	if (grand_parent == server->view_tree) {
		last_view = node_view_from_node(node);
	}
	if (node->parent == server->view_tree_always_on_top) {
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
	int padding = LEFT_COL_SPACE - pos - strlen(type);
	if (!pos) {
		padding += 3;
	}
	printf("%s %*c %4d  %4d  [%p]\n", type, padding, ' ', x, y, node);

	if ((IGNORE_MENU && node == &server->menu_tree->node)
			|| (IGNORE_SSD && last_view
			&& ssd_debug_is_root_node(last_view->ssd, node))) {
		printf("%*c%s\n", pos + 4 + INDENT_SIZE, ' ', "<skipping children>");
		return;
	}

	if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_node *child;
		struct wlr_scene_tree *tree = lab_scene_tree_from_node(node);
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
