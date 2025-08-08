// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <cairo.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>
#include "common/array.h"
#include "common/buf.h"
#include "common/font.h"
#include "common/lab-scene-rect.h"
#include "common/scaled-font-buffer.h"
#include "common/scaled-icon-buffer.h"
#include "common/scene-helpers.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "osd.h"
#include "output.h"
#include "theme.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"

struct osd_classic_scene_item {
	struct view *view;
	struct wlr_scene_node *highlight_outline;
};

static void
osd_classic_create(struct output *output, struct wl_array *views)
{
	assert(!output->osd_scene.tree);

	struct server *server = output->server;
	struct theme *theme = server->theme;
	bool show_workspace = wl_list_length(&rc.workspace_config.workspaces) > 1;
	const char *workspace_name = server->workspaces.current->name;

	int w = theme->osd_window_switcher_width;
	if (theme->osd_window_switcher_width_is_percent) {
		w = output->wlr_output->width
			* theme->osd_window_switcher_width / 100;
	}
	int h = wl_array_len(views) * rc.theme->osd_window_switcher_item_height
		+ 2 * rc.theme->osd_border_width
		+ 2 * rc.theme->osd_window_switcher_padding;
	if (show_workspace) {
		/* workspace indicator */
		h += theme->osd_window_switcher_item_height;
	}

	output->osd_scene.tree = wlr_scene_tree_create(output->osd_tree);

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
	lab_scene_rect_create(output->osd_scene.tree, &bg_opts);

	int y = theme->osd_border_width + theme->osd_window_switcher_padding;

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
			scaled_font_buffer_create(output->osd_scene.tree);
		wlr_scene_node_set_position(&font_buffer->scene_buffer->node,
			x, y + (theme->osd_window_switcher_item_height
				- font_height(&font)) / 2);
		scaled_font_buffer_update(font_buffer, workspace_name, 0,
			&font, text_color, bg_color);
		y += theme->osd_window_switcher_item_height;
	}

	struct buf buf = BUF_INIT;
	int nr_fields = wl_list_length(&rc.window_switcher.fields);

	/* This is the width of the area available for text fields */
	int field_widths_sum = w - 2 * theme->osd_border_width
		- 2 * theme->osd_window_switcher_padding
		- 2 * theme->osd_window_switcher_item_active_border_width
		- (nr_fields + 1) * theme->osd_window_switcher_item_padding_x;
	if (field_widths_sum <= 0) {
		wlr_log(WLR_ERROR, "Not enough spaces for osd contents");
		goto error;
	}

	/* Draw text for each node */
	struct view **view;
	wl_array_for_each(view, views) {
		struct osd_classic_scene_item *item =
			wl_array_add(&output->osd_scene.items, sizeof(*item));
		item->view = *view;
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
		int x = theme->osd_border_width
			+ theme->osd_window_switcher_padding
			+ theme->osd_window_switcher_item_active_border_width
			+ theme->osd_window_switcher_item_padding_x;
		struct wlr_scene_tree *item_root =
			wlr_scene_tree_create(output->osd_scene.tree);

		struct window_switcher_field *field;
		wl_list_for_each(field, &rc.window_switcher.fields, link) {
			int field_width = field_widths_sum * field->width / 100.0;
			struct wlr_scene_node *node = NULL;
			int height = -1;

			if (field->content == LAB_FIELD_ICON) {
				int icon_size = MIN(field_width,
					theme->osd_window_switcher_item_icon_size);
				struct scaled_icon_buffer *icon_buffer =
					scaled_icon_buffer_create(item_root,
						server, icon_size, icon_size);
				scaled_icon_buffer_set_view(icon_buffer, *view);
				node = &icon_buffer->scene_buffer->node;
				height = icon_size;
			} else {
				buf_clear(&buf);
				osd_field_get_content(field, &buf, *view);

				if (!string_null_or_empty(buf.data)) {
					struct scaled_font_buffer *font_buffer =
						scaled_font_buffer_create(item_root);
					scaled_font_buffer_update(font_buffer,
						buf.data, field_width,
						&rc.font_osd, text_color, bg_color);
					node = &font_buffer->scene_buffer->node;
					height = font_height(&rc.font_osd);
				}
			}

			if (node) {
				int item_height =
					theme->osd_window_switcher_item_height;
				wlr_scene_node_set_position(node,
					x, y + (item_height - height) / 2);
			}
			x += field_width + theme->osd_window_switcher_item_padding_x;
		}

		/* Highlight around selected window's item */
		int highlight_x = theme->osd_border_width
				+ theme->osd_window_switcher_padding;
		struct lab_scene_rect_options highlight_opts = {
			.border_colors = (float *[1]) {text_color},
			.nr_borders = 1,
			.border_width =
				theme->osd_window_switcher_item_active_border_width,
			.width = w - 2 * theme->osd_border_width
				- 2 * theme->osd_window_switcher_padding,
			.height = theme->osd_window_switcher_item_height,
		};

		struct lab_scene_rect *highlight_rect = lab_scene_rect_create(
			output->osd_scene.tree, &highlight_opts);
		item->highlight_outline = &highlight_rect->tree->node;
		wlr_scene_node_set_position(item->highlight_outline, highlight_x, y);
		wlr_scene_node_set_enabled(item->highlight_outline, false);

		y += theme->osd_window_switcher_item_height;
	}
	buf_reset(&buf);

error:;
	/* Center OSD */
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	wlr_scene_node_set_position(&output->osd_scene.tree->node,
		usable.x + usable.width / 2 - w / 2,
		usable.y + usable.height / 2 - h / 2);
}

static void
osd_classic_update(struct output *output)
{
	struct osd_classic_scene_item *item;
	wl_array_for_each(item, &output->osd_scene.items) {
		wlr_scene_node_set_enabled(item->highlight_outline,
			item->view == output->server->osd_state.cycle_view);
	}
}

struct osd_impl osd_classic_impl = {
	.create = osd_classic_create,
	.update = osd_classic_update,
};
