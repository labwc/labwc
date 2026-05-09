// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config/dialog.h"
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include "common/lab-scene-rect.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "theme.h"

static void
dialog_create(void)
{
	struct theme *theme = rc.theme;
	float *text_color = theme->osd_label_text_color;
	float *bg_color = theme->osd_bg_color;

	struct output *output = output_nearest_to_cursor();
	struct wlr_box output_box = output_usable_area_in_layout_coords(output);

	int border = theme->osd_border_width;
	int padding = rc.font_osd.size / 2; /* used for padding and line gap */
	int x = output_box.x + output_box.width / 4;
	int y = output_box.y + output_box.height / 10;
	int width = output_box.width * 0.5;
	int height = output_box.height * 0.8;
	int inner_w = width - 2 * border;
	int inner_h = height - 2 * border;
	int dx = border + padding;
	int dy = border + padding;
	struct wlr_scene_tree *dialog = lab_wlr_scene_tree_create(server.dialog_tree);
	node_descriptor_create(&dialog->node, LAB_NODE_CONFIG_DIALOG, NULL, NULL);

	wlr_scene_node_set_position(&dialog->node, x, y);
	struct wlr_scene_rect *border_rect = lab_wlr_scene_rect_create(dialog, width, height,
		theme->osd_border_color);
	wlr_scene_node_set_position(&border_rect->node, 0, 0);
	struct wlr_scene_tree *dialog_tree = lab_wlr_scene_tree_create(dialog);
	wlr_scene_node_set_position(&dialog_tree->node, border, border);
	struct wlr_scene_rect *dialog_rect = lab_wlr_scene_rect_create(dialog_tree,
		inner_w, inner_h, bg_color);
	wlr_scene_node_set_position(&dialog_rect->node, 0, 0);

	struct scaled_font_buffer *font_buf = scaled_font_buffer_create(dialog_tree);
	struct font title_font = rc.font_osd;
	title_font.size *= 1.15;
	scaled_font_buffer_update(font_buf, "Config log (Click to dismiss)", inner_w,
		&title_font, text_color, bg_color);
	wlr_scene_node_set_position(&font_buf->scene_buffer->node, dx, dy);
	dy += font_buf->height + padding;

	struct log_item *item;
	wl_list_for_each(item, &rc.error_logs, link) {
		font_buf = scaled_font_buffer_create(dialog_tree);
		scaled_font_buffer_update(font_buf, item->text, inner_w, &rc.font_osd,
			text_color, bg_color);
		wlr_scene_node_set_position(&font_buf->scene_buffer->node, dx, dy);
		dy += font_buf->height + padding;
	}
	wlr_scene_node_set_enabled(&server.dialog_tree->node, true);
}

void
dialog_create_callback(void *data)
{
	dialog_create();
}

void
dialog_destroy(struct wlr_scene_node *dialog)
{
	struct wlr_scene_node *child, *tmp;
	wl_list_for_each_safe(child, tmp, &server.dialog_tree->children, link) {
		if (!dialog) {
			wlr_scene_node_destroy(child);
			continue;
		}
		if (dialog == child) {
			wlr_scene_node_destroy(child);
			break;
		}
	}
}
