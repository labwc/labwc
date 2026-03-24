// SPDX-License-Identifier: GPL-2.0-only
#include "common/lab-scene-rect.h"
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "common/mem.h"
#include "common/scene-helpers.h"

struct border_scene {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *top, *bottom, *left, *right;
};

static void
handle_node_destroy(struct wl_listener *listener, void *data)
{
	struct lab_scene_rect *rect = wl_container_of(listener, rect, node_destroy);
	wl_list_remove(&rect->node_destroy.link);
	free(rect->borders);
	free(rect);
}

struct lab_scene_rect *
lab_scene_rect_create(struct wlr_scene_tree *parent,
		struct lab_scene_rect_options *opts)
{
	struct lab_scene_rect *rect = znew(*rect);
	rect->border_width = opts->border_width;
	rect->nr_borders = opts->nr_borders;
	rect->borders = znew_n(rect->borders[0], opts->nr_borders);
	rect->tree = lab_wlr_scene_tree_create(parent);

	if (opts->bg_color) {
		rect->fill = lab_wlr_scene_rect_create(rect->tree, 0, 0, opts->bg_color);
	}

	for (int i = 0; i < rect->nr_borders; i++) {
		struct border_scene *border = &rect->borders[i];
		float *color = opts->border_colors[i];
		
		/* From Pull request 3382 */
		float r = color[0];
		float g = color[1];
		float b = color[2];
		float a = color[3];

		/* highlight */
		float r1 = r * 5 / 4;
		if (r1 > a) r1=a;
		float g1 = g * 5 / 4;
		if (g1 > a) g1=a;
		float b1 = b * 5 / 4;
		if (b1 > a) b1=a;

		/* darker outline */
		float r0 = r / 2;
		float g0 = g / 2;
		float b0 = b / 2;

		const float highlight[4] = {r1, g1, b1, a};
		const float lowlight[4] = {r0, g0, b0, a};
		border->tree = lab_wlr_scene_tree_create(rect->tree);
		border->top = lab_wlr_scene_rect_create(border->tree, 0, 0, highlight);
		border->right = lab_wlr_scene_rect_create(border->tree, 0, 0,lowlight);
		border->bottom = lab_wlr_scene_rect_create(border->tree, 0, 0,lowlight);
		border->left = lab_wlr_scene_rect_create(border->tree, 0, 0,highlight);
		
	}

	rect->node_destroy.notify = handle_node_destroy;
	wl_signal_add(&rect->tree->node.events.destroy, &rect->node_destroy);

	lab_scene_rect_set_size(rect, opts->width, opts->height);

	return rect;
}

static void
resize_border(struct border_scene *border, int border_width, int width, int height)
{
	/*
	 * The border is drawn like below:
	 *
	 * <--width-->
	 * +---------+   ^
	 * +-+-----+-+   |
	 * | |     | | height
	 * | |     | |   |
	 * +-+-----+-+   |
	 * +---------+   v
	 */

	if ((width < border_width * 2) || (height < border_width * 2)) {
		wlr_scene_node_set_enabled(&border->tree->node, false);
		return;
	}
	wlr_scene_node_set_enabled(&border->tree->node, true);

	wlr_scene_node_set_position(&border->top->node, 0, 0);
	wlr_scene_node_set_position(&border->bottom->node, 0, height - border_width);
	wlr_scene_node_set_position(&border->left->node, 0, border_width);
	wlr_scene_node_set_position(&border->right->node, width - border_width, border_width);

	wlr_scene_rect_set_size(border->top, width, border_width);
	wlr_scene_rect_set_size(border->bottom, width, border_width);
	wlr_scene_rect_set_size(border->left, border_width, height - border_width * 2);
	wlr_scene_rect_set_size(border->right, border_width, height - border_width * 2);
}

void
lab_scene_rect_set_size(struct lab_scene_rect *rect, int width, int height)
{
	assert(rect);
	int border_width = rect->border_width;

	for (int i = 0; i < rect->nr_borders; i++) {
		struct border_scene *border = &rect->borders[i];
		resize_border(border, border_width,
			width - 2 * border_width * i,
			height - 2 * border_width * i);
		wlr_scene_node_set_position(&border->tree->node,
			i * border_width, i * border_width);
	}

	if (rect->fill) {
		wlr_scene_rect_set_size(rect->fill, width, height);
	}
}
