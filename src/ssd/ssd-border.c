// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/macros.h"
#include "common/mem.h"
#include "labwc.h"
#include "ssd.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

void
ssd_border_create(struct ssd *ssd)
{
	assert(ssd);
	assert(!ssd->border.tree);

	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
#if 0
	/* XXX: not using with textured borders */
	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
	int full_width = width + 2 * theme->border_width;
	int corner_width = ssd_get_corner_width();
#endif

	ssd->border.tree = wlr_scene_tree_create(ssd->tree);
#if 0
	/* XXX: eliminated this offset */
	wlr_scene_node_set_position(&ssd->border.tree->node, -theme->border_width, 0);
#endif

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_border_subtree *subtree = &ssd->border.subtrees[active];
		subtree->tree = wlr_scene_tree_create(ssd->border.tree);
		struct wlr_scene_tree *parent = subtree->tree;
		wlr_scene_node_set_enabled(&parent->node, active);
		float *color = theme->window[active].title_bg.color;

#if 0
		/* XXX: old solid color borders */
		subtree->left = wlr_scene_rect_create(parent,
			theme->border_width, height, color);
		wlr_scene_node_set_position(&subtree->left->node, 0, 0);

		subtree->right = wlr_scene_rect_create(parent,
			theme->border_width, height, color);
		wlr_scene_node_set_position(&subtree->right->node,
			theme->border_width + width, 0);

		subtree->bottom = wlr_scene_rect_create(parent,
			full_width, theme->border_width, color);
		wlr_scene_node_set_position(&subtree->bottom->node,
			0, height);

		subtree->top = wlr_scene_rect_create(parent,
			MAX(width - 2 * corner_width, 0), theme->border_width, color);
		wlr_scene_node_set_position(&subtree->top->node,
			theme->border_width + corner_width,
			-(ssd->titlebar.height + theme->border_width));
#endif

		uint8_t r = color[0] * 255;
		uint8_t g = color[1] * 255;
		uint8_t b = color[2] * 255;
		uint8_t a = color[3] * 255;

		/* darker outline */
		uint8_t r0 = r / 2;
		uint8_t g0 = g / 2;
		uint8_t b0 = b / 2;

		/* highlight */
		uint8_t r1 = MIN(r * 5 / 4, a);
		uint8_t g1 = MIN(g * 5 / 4, a);
		uint8_t b1 = MIN(b * 5 / 4, a);

		uint32_t col = ((uint32_t)a << 24) | ((uint32_t)r << 16)
			| ((uint32_t)g << 8) | b;
		uint32_t col0 = ((uint32_t)a << 24) | ((uint32_t)r0 << 16)
			| ((uint32_t)g0 << 8) | b0;
		uint32_t col1 = ((uint32_t)a << 24) | ((uint32_t)r1 << 16)
			| ((uint32_t)g1 << 8) | b1;

		/* top and left start out the same */
		uint32_t *left_data = znew_n(uint32_t, BORDER_PX_SIDE);
		uint32_t *top_data = znew_n(uint32_t, BORDER_PX_TOP);
		left_data[0] = top_data[0] = col0;
		left_data[1] = top_data[1] = col1;
		for (int i = 2; i < BORDER_PX_SIDE; i++) {
			left_data[i] = col;
		}
		for (int i = 2; i < BORDER_PX_TOP; i++) {
			top_data[i] = col;
		}

		/* bottom and right are identical */
		uint32_t *right_data = znew_n(uint32_t, BORDER_PX_SIDE);
		uint32_t *bottom_data = znew_n(uint32_t, BORDER_PX_SIDE);
		for (int i = 0; i < BORDER_PX_SIDE - 1; i++) {
			right_data[i] = col;
			bottom_data[i] = col;
		}
		right_data[BORDER_PX_SIDE - 1] = col0;
		bottom_data[BORDER_PX_SIDE - 1] = col0;

		/* order matters here since the border overlap */
		struct lab_data_buffer *bottom_buffer =
			buffer_create_from_data(bottom_data, 1, BORDER_PX_SIDE, 4);
		subtree->bottom = wlr_scene_buffer_create(parent, &bottom_buffer->base);
		wlr_buffer_drop(&bottom_buffer->base);

		struct lab_data_buffer *right_buffer =
			buffer_create_from_data(right_data, BORDER_PX_SIDE, 1,
				4 * BORDER_PX_SIDE);
		subtree->right = wlr_scene_buffer_create(parent, &right_buffer->base);
		wlr_buffer_drop(&right_buffer->base);

		struct lab_data_buffer *left_buffer =
			buffer_create_from_data(left_data, BORDER_PX_SIDE, 1,
				4 * BORDER_PX_SIDE);
		subtree->left = wlr_scene_buffer_create(parent, &left_buffer->base);
		wlr_buffer_drop(&left_buffer->base);

		struct lab_data_buffer *top_buffer =
			buffer_create_from_data(top_data, 1, BORDER_PX_TOP, 4);
		subtree->top = wlr_scene_buffer_create(parent, &top_buffer->base);
		wlr_buffer_drop(&top_buffer->base);
	}

	/* Lower textured borders below titlebar for overlap */
	wlr_scene_node_lower_to_bottom(&ssd->border.tree->node);

	if (view->maximized == VIEW_AXIS_BOTH) {
		wlr_scene_node_set_enabled(&ssd->border.tree->node, false);
	}

	if (view->current.width > 0 && view->current.height > 0) {
		/*
		 * The SSD is recreated by a Reconfigure request
		 * thus we may need to handle squared corners.
		 */
		ssd_border_update(ssd);
	}
}

void
ssd_border_update(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->border.tree);

	struct view *view = ssd->view;
	if (view->maximized == VIEW_AXIS_BOTH
			&& ssd->border.tree->node.enabled) {
		/* Disable borders on maximize */
		wlr_scene_node_set_enabled(&ssd->border.tree->node, false);
		ssd->margin = ssd_thickness(ssd->view);
	}

	if (view->maximized == VIEW_AXIS_BOTH) {
		return;
	} else if (!ssd->border.tree->node.enabled) {
		/* And re-enabled them when unmaximized */
		wlr_scene_node_set_enabled(&ssd->border.tree->node, true);
		ssd->margin = ssd_thickness(ssd->view);
	}

	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
#if 0
	/* XXX: ignoring lots of possible cases */
	int full_width = width + 2 * theme->border_width;
	int corner_width = ssd_get_corner_width();

	/*
	 * From here on we have to cover the following border scenarios:
	 * Non-tiled (partial border, rounded corners):
	 *    _____________
	 *   o           oox
	 *  |---------------|
	 *  |_______________|
	 *
	 * Tiled (full border, squared corners):
	 *   _______________
	 *  |o           oox|
	 *  |---------------|
	 *  |_______________|
	 *
	 * Tiled or non-tiled with zero title height (full boarder, no title):
	 *   _______________
	 *  |_______________|
	 */

	int side_height = ssd->state.was_squared
		? height + ssd->titlebar.height
		: height;
	int side_y = ssd->state.was_squared
		? -ssd->titlebar.height
		: 0;
	int top_width = ssd->titlebar.height <= 0 || ssd->state.was_squared
		? full_width
		: MAX(width - 2 * corner_width, 0);
	int top_x = ssd->titlebar.height <= 0 || ssd->state.was_squared
		? 0
		: theme->border_width + corner_width;
#endif
	int title_h = ssd->titlebar.height;
	int side_y = -title_h - (BORDER_PX_TOP - 1);
	int side_height = title_h + height + (BORDER_PX_TOP - 1) + (BORDER_PX_SIDE - 1);
	int top_x = -(BORDER_PX_SIDE - 1);
	int top_width = width + 2 * (BORDER_PX_SIDE - 1);

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_border_subtree *subtree = &ssd->border.subtrees[active];

#if 0
		/* XXX: old solid color borders */
		wlr_scene_rect_set_size(subtree->left,
			theme->border_width, side_height);
		wlr_scene_node_set_position(&subtree->left->node,
			0, side_y);

		wlr_scene_rect_set_size(subtree->right,
			theme->border_width, side_height);
		wlr_scene_node_set_position(&subtree->right->node,
			theme->border_width + width, side_y);

		wlr_scene_rect_set_size(subtree->bottom,
			full_width, theme->border_width);
		wlr_scene_node_set_position(&subtree->bottom->node,
			0, height);

		wlr_scene_rect_set_size(subtree->top,
			top_width, theme->border_width);
		wlr_scene_node_set_position(&subtree->top->node,
			top_x, -(ssd->titlebar.height + theme->border_width));
#endif

		wlr_scene_node_set_position(&subtree->left->node,
			-BORDER_PX_SIDE, side_y);
		wlr_scene_buffer_set_dest_size(subtree->left,
			BORDER_PX_SIDE, side_height);

		wlr_scene_node_set_position(&subtree->right->node,
			width, side_y);
		wlr_scene_buffer_set_dest_size(subtree->right,
			BORDER_PX_SIDE, side_height);

		wlr_scene_node_set_position(&subtree->bottom->node,
			top_x, height);
		wlr_scene_buffer_set_dest_size(subtree->bottom,
			top_width, BORDER_PX_SIDE);

		wlr_scene_node_set_position(&subtree->top->node,
			top_x, -title_h - BORDER_PX_TOP);
		wlr_scene_buffer_set_dest_size(subtree->top,
			top_width, BORDER_PX_TOP);
	}
}

void
ssd_border_destroy(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->border.tree);

	wlr_scene_node_destroy(&ssd->border.tree->node);
	ssd->border = (struct ssd_border_scene){0};
}
