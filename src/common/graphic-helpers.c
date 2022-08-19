// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_scene.h>
#include "common/graphic-helpers.h"

static void
multi_rect_destroy_notify(struct wl_listener *listener, void *data)
{
	struct multi_rect *rect = wl_container_of(listener, rect, destroy);
	free(rect);
}

struct multi_rect *
multi_rect_create(struct wlr_scene_tree *parent, float *colors[3], int line_width)
{
	struct multi_rect *rect = calloc(1, sizeof(*rect));
	rect->line_width = line_width;
	rect->tree = wlr_scene_tree_create(parent);
	rect->destroy.notify = multi_rect_destroy_notify;
	wl_signal_add(&rect->tree->node.events.destroy, &rect->destroy);
	for (size_t i = 0; i < 3; i++) {
		rect->top[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		rect->right[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		rect->bottom[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		rect->left[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		wlr_scene_node_set_position(&rect->top[i]->node,
			i * line_width, i * line_width);
		wlr_scene_node_set_position(&rect->left[i]->node,
			i * line_width, i * line_width);
	}
	return rect;
}

void
multi_rect_set_size(struct multi_rect *rect, int width, int height)
{
	assert(rect);
	int line_width = rect->line_width;

	for (size_t i = 0; i < 3; i++) {

		/* Reposition, top and left don't ever change */
		wlr_scene_node_set_position(&rect->right[i]->node,
			width - (i + 1) * line_width, i * line_width);
		wlr_scene_node_set_position(&rect->bottom[i]->node,
			i * line_width, height - (i + 1) * line_width);

		/* Update sizes */
		wlr_scene_rect_set_size(rect->top[i],
			width - i * line_width * 2, line_width);
		wlr_scene_rect_set_size(rect->bottom[i],
			width - i * line_width * 2, line_width);
		wlr_scene_rect_set_size(rect->left[i],
			line_width, height - i * line_width * 2);
		wlr_scene_rect_set_size(rect->right[i],
			line_width, height - i * line_width * 2);
	}
}
