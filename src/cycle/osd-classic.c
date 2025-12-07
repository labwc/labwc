// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/font.h"
#include "common/lab-scene-rect.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "cycle.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "scaled-buffer/scaled-icon-buffer.h"
#include "theme.h"
#include "view.h"
#include "workspaces.h"

struct cycle_osd_classic_item {
	struct cycle_osd_item base;
	struct wlr_scene_tree *normal_tree, *active_tree;
};

static void
create_fields_scene(struct server *server, struct view *view,
		struct wlr_scene_tree *parent, const float *text_color,
		const float *bg_color, int field_widths_sum, int x, int y)
{
	struct theme *theme = server->theme;
	struct window_switcher_classic_theme *switcher_theme =
		&theme->osd_window_switcher_classic;

	struct cycle_osd_field *field;
	wl_list_for_each(field, &rc.window_switcher.fields, link) {
		int field_width = field_widths_sum * field->width / 100.0;
		struct wlr_scene_node *node = NULL;
		int height = -1;

		if (field->content == LAB_FIELD_ICON) {
			int icon_size = MIN(field_width,
				switcher_theme->item_icon_size);
			struct scaled_icon_buffer *icon_buffer =
				scaled_icon_buffer_create(parent,
					server, icon_size, icon_size);
			scaled_icon_buffer_set_view(icon_buffer, view);
			node = &icon_buffer->scene_buffer->node;
			height = icon_size;
		} else {
			struct buf buf = BUF_INIT;
			cycle_osd_field_get_content(field, &buf, view);

			if (!string_null_or_empty(buf.data)) {
				struct scaled_font_buffer *font_buffer =
					scaled_font_buffer_create(parent);
				scaled_font_buffer_update(font_buffer,
					buf.data, field_width,
					&rc.font_osd, text_color, bg_color);
				node = &font_buffer->scene_buffer->node;
				height = font_height(&rc.font_osd);
			}

			buf_reset(&buf);
		}

		if (node) {
			int item_height = switcher_theme->item_height;
			wlr_scene_node_set_position(node,
				x, y + (item_height - height) / 2);
		}
		x += field_width + switcher_theme->item_padding_x;
	}
}

static void
cycle_osd_classic_create(struct output *output)
{
	assert(!output->cycle_osd.tree && wl_list_empty(&output->cycle_osd.items));

	struct server *server = output->server;
	struct theme *theme = server->theme;
	struct window_switcher_classic_theme *switcher_theme =
		&theme->osd_window_switcher_classic;
	int padding = theme->osd_border_width + switcher_theme->padding;
	bool show_workspace = wl_list_length(&rc.workspace_config.workspaces) > 1;
	const char *workspace_name = server->workspaces.current->name;
	int nr_views = wl_list_length(&server->cycle.views);

	struct wlr_box output_box;
	wlr_output_layout_get_box(server->output_layout, output->wlr_output,
		&output_box);

	int w = switcher_theme->width;
	if (switcher_theme->width_is_percent) {
		w = output_box.width * switcher_theme->width / 100;
	}
	int h = nr_views * switcher_theme->item_height + 2 * padding;
	if (show_workspace) {
		/* workspace indicator */
		h += switcher_theme->item_height;
	}

	output->cycle_osd.tree = wlr_scene_tree_create(output->cycle_osd_tree);

	float *text_color = theme->osd_label_text_color;
	float *bg_color = theme->osd_bg_color;

	/* Draw background */
	struct lab_scene_rect_options bg_opts = {
		.border_colors = (float *[1]) {theme->osd_border_color},
		.nr_borders = 1,
		.border_width = theme->osd_border_width,
		.bg_color = bg_color,
		.width = w,
		.height = h,
	};
	lab_scene_rect_create(output->cycle_osd.tree, &bg_opts);

	int y = padding;

	/* Draw workspace indicator */
	if (show_workspace) {
		struct font font = rc.font_osd;
		font.weight = PANGO_WEIGHT_BOLD;

		/* Center workspace indicator on the x axis */
		int x = (w - font_width(&font, workspace_name)) / 2;
		if (x < 0) {
			wlr_log(WLR_ERROR,
				"not enough space for workspace name in osd");
			goto error;
		}

		struct scaled_font_buffer *font_buffer =
			scaled_font_buffer_create(output->cycle_osd.tree);
		wlr_scene_node_set_position(&font_buffer->scene_buffer->node,
			x, y + (switcher_theme->item_height - font_height(&font)) / 2);
		scaled_font_buffer_update(font_buffer, workspace_name, 0,
			&font, text_color, bg_color);
		y += switcher_theme->item_height;
	}

	int nr_fields = wl_list_length(&rc.window_switcher.fields);

	/* This is the width of the area available for text fields */
	int field_widths_sum = w - 2 * padding
		- 2 * switcher_theme->item_active_border_width
		- (nr_fields + 1) * switcher_theme->item_padding_x;
	if (field_widths_sum <= 0) {
		wlr_log(WLR_ERROR, "Not enough spaces for osd contents");
		goto error;
	}

	/* Draw text for each node */
	struct view *view;
	wl_list_for_each(view, &server->cycle.views, cycle_link) {
		struct cycle_osd_classic_item *item = znew(*item);
		wl_list_append(&output->cycle_osd.items, &item->base.link);
		item->base.view = view;
		item->base.tree = wlr_scene_tree_create(output->cycle_osd.tree);
		node_descriptor_create(&item->base.tree->node,
			LAB_NODE_CYCLE_OSD_ITEM, NULL, item);
		/*
		 *    OSD border
		 * +---------------------------------+
		 * |                                 |
		 * |  item border                    |
		 * |+-------------------------------+|
		 * ||                               ||
		 * ||padding between each field     ||
		 * ||| field-1 | field-2 | field-n |||
		 * ||                               ||
		 * ||                               ||
		 * |+-------------------------------+|
		 * |                                 |
		 * |                                 |
		 * +---------------------------------+
		 */
		int x = padding
			+ switcher_theme->item_active_border_width
			+ switcher_theme->item_padding_x;
		item->normal_tree = wlr_scene_tree_create(item->base.tree);
		item->active_tree = wlr_scene_tree_create(item->base.tree);
		wlr_scene_node_set_enabled(&item->active_tree->node, false);

		float *active_bg_color = switcher_theme->item_active_bg_color;
		float *active_border_color = switcher_theme->item_active_border_color;

		/* Highlight around selected window's item */
		struct lab_scene_rect_options highlight_opts = {
			.border_colors = (float *[1]) {active_border_color},
			.nr_borders = 1,
			.border_width = switcher_theme->item_active_border_width,
			.bg_color = active_bg_color,
			.width = w - 2 * padding,
			.height = switcher_theme->item_height,
		};
		struct lab_scene_rect *highlight_rect = lab_scene_rect_create(
			item->active_tree, &highlight_opts);
		wlr_scene_node_set_position(&highlight_rect->tree->node, padding, y);

		/* hitbox for mouse clicks */
		struct wlr_scene_rect *hitbox = wlr_scene_rect_create(item->base.tree,
			w - 2 * padding, switcher_theme->item_height, (float[4]) {0});
		wlr_scene_node_set_position(&hitbox->node, padding, y);

		create_fields_scene(server, view, item->normal_tree,
			text_color, bg_color, field_widths_sum, x, y);
		create_fields_scene(server, view, item->active_tree,
			text_color, active_bg_color, field_widths_sum, x, y);

		y += switcher_theme->item_height;
	}

error:;
	/* Center OSD */
	wlr_scene_node_set_position(&output->cycle_osd.tree->node,
		output_box.x + (output_box.width - w) / 2,
		output_box.y + (output_box.height - h) / 2);
}

static void
cycle_osd_classic_update(struct output *output)
{
	struct cycle_osd_classic_item *item;
	wl_list_for_each(item, &output->cycle_osd.items, base.link) {
		bool active = item->base.view == output->server->cycle.selected_view;
		wlr_scene_node_set_enabled(&item->normal_tree->node, !active);
		wlr_scene_node_set_enabled(&item->active_tree->node, active);
	}
}

struct cycle_osd_impl cycle_osd_classic_impl = {
	.create = cycle_osd_classic_create,
	.update = cycle_osd_classic_update,
};
