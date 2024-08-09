// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "common/scene-helpers.h"
#include "labwc.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

#define FOR_EACH_STATE(ssd, tmp) FOR_EACH(tmp, \
	&(ssd)->border.active, \
	&(ssd)->border.inactive)

void
ssd_border_create(struct ssd *ssd)
{
	assert(ssd);
	assert(!ssd->border.tree);

	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
	int full_width = width + 2 * theme->border_width;
	int button_width = ssd->titlebar.button_width;

	float *color;
	struct wlr_scene_tree *parent;
	struct ssd_sub_tree *subtree;

	ssd->border.tree = wlr_scene_tree_create(ssd->tree);
	wlr_scene_node_set_position(&ssd->border.tree->node, -theme->border_width, 0);

	FOR_EACH_STATE(ssd, subtree) {
		subtree->tree = wlr_scene_tree_create(ssd->border.tree);
		parent = subtree->tree;
		if (subtree == &ssd->border.active) {
			color = theme->window_active_border_color;
		} else {
			color = theme->window_inactive_border_color;
			wlr_scene_node_set_enabled(&parent->node, false);
		}
		wl_list_init(&subtree->parts);
		add_scene_rect(&subtree->parts, LAB_SSD_PART_LEFT, parent,
			theme->border_width, height, 0, 0, color);
		add_scene_rect(&subtree->parts, LAB_SSD_PART_RIGHT, parent,
			theme->border_width, height,
			theme->border_width + width, 0, color);
		add_scene_rect(&subtree->parts, LAB_SSD_PART_BOTTOM, parent,
			full_width, theme->border_width, 0, height, color);
		add_scene_rect(&subtree->parts, LAB_SSD_PART_TOP, parent,
			width - 2 * button_width, theme->border_width,
			theme->border_width + button_width,
			-(ssd->titlebar.height + theme->border_width), color);
	} FOR_EACH_END

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

	struct theme *theme = view->server->theme;

	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
	int full_width = width + 2 * theme->border_width;
	int button_width = ssd->titlebar.button_width;

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

	int side_height = ssd->state.was_tiled_not_maximized
		? height + ssd->titlebar.height
		: height;
	int side_y = ssd->state.was_tiled_not_maximized
		? -ssd->titlebar.height
		: 0;
	int top_width = ssd->titlebar.height <= 0 || ssd->state.was_tiled_not_maximized
		? full_width
		: width - 2 * button_width;
	int top_x = ssd->titlebar.height <= 0 || ssd->state.was_tiled_not_maximized
		? 0
		: theme->border_width + button_width;

	struct ssd_part *part;
	struct wlr_scene_rect *rect;
	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		wl_list_for_each(part, &subtree->parts, link) {
			rect = wlr_scene_rect_from_node(part->node);
			switch (part->type) {
			case LAB_SSD_PART_LEFT:
				wlr_scene_rect_set_size(rect,
					theme->border_width,
					side_height);
				wlr_scene_node_set_position(part->node,
					0,
					side_y);
				continue;
			case LAB_SSD_PART_RIGHT:
				wlr_scene_rect_set_size(rect,
					theme->border_width,
					side_height);
				wlr_scene_node_set_position(part->node,
					theme->border_width + width,
					side_y);
				continue;
			case LAB_SSD_PART_BOTTOM:
				wlr_scene_rect_set_size(rect,
					full_width,
					theme->border_width);
				wlr_scene_node_set_position(part->node,
					0,
					height);
				continue;
			case LAB_SSD_PART_TOP:
				wlr_scene_rect_set_size(rect,
					top_width,
					theme->border_width);
				wlr_scene_node_set_position(part->node,
					top_x,
					-(ssd->titlebar.height + theme->border_width));
				continue;
			default:
				continue;
			}
		}
	} FOR_EACH_END
}

void
ssd_border_destroy(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->border.tree);

	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		ssd_destroy_parts(&subtree->parts);
		wlr_scene_node_destroy(&subtree->tree->node);
		subtree->tree = NULL;
	} FOR_EACH_END

	wlr_scene_node_destroy(&ssd->border.tree->node);
	ssd->border.tree = NULL;
}

#undef FOR_EACH_STATE
