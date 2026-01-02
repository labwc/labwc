// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "common/lab-scene-rect.h"
#include "labwc.h"
#include "cycle.h"
#include "output.h"

void
cycle_osd_scroll_init(struct cycle_osd_output *osd_output, struct wlr_box bar_area,
		int delta_y, int nr_cols, int nr_rows, int nr_visible_rows,
		float *border_color, float *bg_color)
{
	if (nr_visible_rows >= nr_rows) {
		/* OSD doesn't have so many windows to scroll through */
		return;
	}

	struct cycle_osd_scroll_context *scroll = &osd_output->scroll;
	scroll->nr_cols = nr_cols;
	scroll->nr_rows = nr_rows;
	scroll->nr_visible_rows = nr_visible_rows;
	scroll->top_row_idx = 0;
	scroll->bar_area = bar_area;
	scroll->delta_y = delta_y;
	scroll->bar_tree = wlr_scene_tree_create(osd_output->tree);
	wlr_scene_node_set_position(&scroll->bar_tree->node,
		bar_area.x, bar_area.y);

	struct lab_scene_rect_options scrollbar_opts = {
		.border_colors = (float *[1]) { border_color },
		.nr_borders = 1,
		.border_width = 1,
		.bg_color = bg_color,
		.width = bar_area.width,
		.height = bar_area.height * nr_visible_rows / nr_rows,
	};
	scroll->bar = lab_scene_rect_create(scroll->bar_tree, &scrollbar_opts);
}

static int
get_cycle_idx(struct cycle_osd_output *osd_output)
{
	struct server *server = osd_output->output->server;

	int idx = 0;
	struct cycle_osd_item *item;
	wl_list_for_each(item, &osd_output->items, link) {
		if (item->view == server->cycle.selected_view) {
			return idx;
		}
		idx++;
	}
	assert(false && "selected view not found in items");
	return -1;
}

void
cycle_osd_scroll_update(struct cycle_osd_output *osd_output)
{
	struct cycle_osd_scroll_context *scroll = &osd_output->scroll;
	if (!scroll->bar) {
		return;
	}

	int cycle_idx = get_cycle_idx(osd_output);

	/* Update the range of visible rows */
	int bottom_row_idx = scroll->top_row_idx + scroll->nr_visible_rows;
	while (cycle_idx < scroll->top_row_idx * scroll->nr_cols) {
		scroll->top_row_idx--;
		bottom_row_idx--;
	}
	while (cycle_idx >= bottom_row_idx * scroll->nr_cols) {
		scroll->top_row_idx++;
		bottom_row_idx++;
	}

	/* Vertically move scrollbar by (bar height) / (# of total rows) */
	wlr_scene_node_set_position(&scroll->bar->tree->node, 0,
		scroll->bar_area.height * scroll->top_row_idx / scroll->nr_rows);
	/* Vertically move items */
	wlr_scene_node_set_position(&osd_output->items_tree->node, 0,
		-scroll->delta_y * scroll->top_row_idx);

	/* Hide items outside of visible area */
	int idx = 0;
	struct cycle_osd_item *item;
	wl_list_for_each(item, &osd_output->items, link) {
		bool visible = idx >= scroll->top_row_idx * scroll->nr_cols
			&& idx < bottom_row_idx * scroll->nr_cols;
		wlr_scene_node_set_enabled(&item->tree->node, visible);
		idx++;
	}
}
