// SPDX-License-Identifier: GPL-2.0-only

#include "resize-outlines.h"
#include <wlr/types/wlr_scene.h>
#include "common/lab-scene-rect.h"
#include "ssd.h"
#include "labwc.h"

bool
resize_outlines_enabled(struct view *view)
{
	return view->resize_outlines.rect
		&& view->resize_outlines.rect->tree->node.enabled;
}

void
resize_outlines_update(struct view *view, struct wlr_box new_geo)
{
	struct resize_outlines *outlines = &view->resize_outlines;

	if (!outlines->rect) {
		struct lab_scene_rect_options opts = {
			.border_colors = (float *[3]) {
				view->server->theme->osd_bg_color,
				view->server->theme->osd_label_text_color,
				view->server->theme->osd_bg_color,
			},
			.nr_borders = 3,
			.border_width = 1,
		};
		outlines->rect = lab_scene_rect_create(view->scene_tree, &opts);
	}

	struct border margin = ssd_get_margin(view->ssd);
	struct wlr_box box = {
		.x = new_geo.x - margin.left,
		.y = new_geo.y - margin.top,
		.width = new_geo.width + margin.left + margin.right,
		.height = new_geo.height + margin.top + margin.bottom,
	};
	lab_scene_rect_set_size(outlines->rect, box.width, box.height);
	wlr_scene_node_set_position(&outlines->rect->tree->node,
		box.x - view->current.x, box.y - view->current.y);
	wlr_scene_node_set_enabled(
		&view->resize_outlines.rect->tree->node, true);

	outlines->view_geo = new_geo;

	resize_indicator_update(view);
}

void
resize_outlines_finish(struct view *view)
{
	view_move_resize(view, view->resize_outlines.view_geo);
	wlr_scene_node_set_enabled(
		&view->resize_outlines.rect->tree->node, false);
}
