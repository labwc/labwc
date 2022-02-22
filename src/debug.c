// SPDX-License-Identifier: GPL-2.0-only
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "labwc.h"

#define HEADER_CHARS "------------------------------"

#define INDENT_SIZE 3
#define IGNORE_SSD true
#define IGNORE_MENU true
#define LEFT_COL_SPACE 35

static const char *
get_node_type(enum wlr_scene_node_type type)
{
	switch (type) {
	case WLR_SCENE_NODE_ROOT:
		return "root";
	case WLR_SCENE_NODE_TREE:
		return "tree";
	case WLR_SCENE_NODE_SURFACE:
		return "surface";
	case WLR_SCENE_NODE_RECT:
		return "rect";
	case WLR_SCENE_NODE_BUFFER:
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
	if (!view || !view->ssd.tree) {
		return NULL;
	}
	if (node == &view->ssd.tree->node) {
		return "view->ssd";
	}
	if (node == &view->ssd.titlebar.active.tree->node) {
		return "titlebar.active";
	}
	if (node == &view->ssd.titlebar.inactive.tree->node) {
		return "titlebar.inactive";
	}
	if (node == &view->ssd.border.active.tree->node) {
		return "border.active";
	}
	if (node == &view->ssd.border.inactive.tree->node) {
		return "border.inactive";
	}
	if (node == &view->ssd.extents.tree->node) {
		return "extents";
	}
	return NULL;
}

static const char *
get_special(struct server *server, struct wlr_scene_node *node,
	struct view **last_view)
{
	if (node == &server->scene->node) {
		return "server->scene";
	}
	if (node == &server->osd_tree->node) {
		return "server->osd_tree";
	}
	if (node == &server->menu_tree->node) {
		return "server->menu_tree";
	}
	if (node->parent == &server->scene->node) {
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
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
	if (node == &server->view_tree->node) {
		return "server->view_tree";
	}
	if (node->parent == &server->view_tree->node) {
		*last_view = node->data;
	}
	const char *view_part = get_view_part(*last_view, node);
	if (view_part) {
		return view_part;
	}
	return get_node_type(node->type);
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
	static struct view *view;
	const char *type = get_special(server, node, &view);

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
			|| (IGNORE_SSD && view && view->ssd.tree
			&& node == &view->ssd.tree->node)) {
		printf("%*c%s\n", pos + 4 + INDENT_SIZE, ' ', "<skipping children>");
		return;
	}
	struct wlr_scene_node *child;
	wl_list_for_each(child, &node->state.children, state.link) {
		dump_tree(server, child, pos + INDENT_SIZE,
			x + child->state.x, y + child->state.y);
	}
}

void
debug_dump_scene(struct server *server)
{
	printf("\n");
	dump_tree(server, &server->scene->node, 0, 0, 0);
	printf("\n");
}
