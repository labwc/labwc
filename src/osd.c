// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <cairo.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>
#include "common/array.h"
#include "common/buf.h"
#include "common/font.h"
#include "common/macros.h"
#include "common/scaled-font-buffer.h"
#include "common/scaled-icon-buffer.h"
#include "common/scaled-rect-buffer.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "osd.h"
#include "theme.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"

struct osd_scene_item {
	struct view *view;
	struct wlr_scene_node *highlight_outline;
};

static void update_osd(struct server *server);

static void
destroy_osd_scenes(struct server *server)
{
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_scene_node_destroy(&output->osd_scene.tree->node);
		output->osd_scene.tree = NULL;

		wl_array_release(&output->osd_scene.items);
		wl_array_init(&output->osd_scene.items);
	}
}

static void
osd_update_preview_outlines(struct view *view)
{
	/* Create / Update preview outline tree */
	struct server *server = view->server;
	struct multi_rect *rect = view->server->osd_state.preview_outline;
	if (!rect) {
		int line_width = server->theme->osd_window_switcher_preview_border_width;
		float *colors[] = {
			server->theme->osd_window_switcher_preview_border_color[0],
			server->theme->osd_window_switcher_preview_border_color[1],
			server->theme->osd_window_switcher_preview_border_color[2],
		};
		rect = multi_rect_create(&server->scene->tree, colors, line_width);
		wlr_scene_node_place_above(&rect->tree->node, &server->menu_tree->node);
		server->osd_state.preview_outline = rect;
	}

	struct wlr_box geo = ssd_max_extents(view);
	multi_rect_set_size(rect, geo.width, geo.height);
	wlr_scene_node_set_position(&rect->tree->node, geo.x, geo.y);
}

/*
 * Returns the view to select next in the window switcher.
 * If !start_view, the second focusable view is returned.
 */
static struct view *
get_next_cycle_view(struct server *server, struct view *start_view,
		enum lab_cycle_dir dir)
{
	struct view *(*iter)(struct wl_list *head, struct view *view,
		enum lab_view_criteria criteria);
	bool forwards = dir == LAB_CYCLE_DIR_FORWARD;
	iter = forwards ? view_next_no_head_stop : view_prev_no_head_stop;

	enum lab_view_criteria criteria = rc.window_switcher.criteria;

	/*
	 * Views are listed in stacking order, topmost first.  Usually the
	 * topmost view is already focused, so when iterating in the forward
	 * direction we pre-select the view second from the top:
	 *
	 *   View #1 (on top, currently focused)
	 *   View #2 (pre-selected)
	 *   View #3
	 *   ...
	 */
	if (!start_view && forwards) {
		start_view = iter(&server->views, NULL, criteria);
	}

	return iter(&server->views, start_view, criteria);
}

void
osd_on_view_destroy(struct view *view)
{
	assert(view);
	struct osd_state *osd_state = &view->server->osd_state;

	if (view->server->input_mode != LAB_INPUT_STATE_WINDOW_SWITCHER) {
		/* OSD not active, no need for clean up */
		return;
	}

	if (osd_state->cycle_view == view) {
		/*
		 * If we are the current OSD selected view, cycle
		 * to the next because we are dying.
		 */

		/* Also resets preview node */
		osd_state->cycle_view = get_next_cycle_view(view->server,
			osd_state->cycle_view, LAB_CYCLE_DIR_BACKWARD);

		/*
		 * If we cycled back to ourselves, then we have no more windows.
		 * Just close the OSD for good.
		 */
		if (osd_state->cycle_view == view || !osd_state->cycle_view) {
			/* osd_finish() additionally resets cycle_view to NULL */
			osd_finish(view->server);
		}
	}

	if (osd_state->cycle_view) {
		/* Recreate the OSD to reflect the view has now gone. */
		destroy_osd_scenes(view->server);
		update_osd(view->server);
	}

	if (view->scene_tree) {
		struct wlr_scene_node *node = &view->scene_tree->node;
		if (osd_state->preview_anchor == node) {
			/*
			 * If we are the anchor for the current OSD selected view,
			 * replace the anchor with the node before us.
			 */
			osd_state->preview_anchor = lab_wlr_scene_get_prev_node(node);
		}
	}
}

static void
restore_preview_node(struct server *server)
{
	struct osd_state *osd_state = &server->osd_state;
	if (osd_state->preview_node) {
		wlr_scene_node_reparent(osd_state->preview_node,
			osd_state->preview_parent);

		if (osd_state->preview_anchor) {
			wlr_scene_node_place_above(osd_state->preview_node,
				osd_state->preview_anchor);
		} else {
			/* Selected view was the first node */
			wlr_scene_node_lower_to_bottom(osd_state->preview_node);
		}

		/* Node was disabled / minimized before, disable again */
		if (!osd_state->preview_was_enabled) {
			wlr_scene_node_set_enabled(osd_state->preview_node, false);
		}
		osd_state->preview_node = NULL;
		osd_state->preview_parent = NULL;
		osd_state->preview_anchor = NULL;
	}
}

void
osd_begin(struct server *server, enum lab_cycle_dir direction)
{
	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		return;
	}

	server->osd_state.cycle_view = get_next_cycle_view(server,
		server->osd_state.cycle_view, direction);

	seat_focus_override_begin(&server->seat,
		LAB_INPUT_STATE_WINDOW_SWITCHER, LAB_CURSOR_DEFAULT);
	update_osd(server);

	/* Update cursor, in case it is within the area covered by OSD */
	cursor_update_focus(server);
}

void
osd_cycle(struct server *server, enum lab_cycle_dir direction)
{
	assert(server->input_mode == LAB_INPUT_STATE_WINDOW_SWITCHER);

	server->osd_state.cycle_view = get_next_cycle_view(server,
		server->osd_state.cycle_view, direction);
	update_osd(server);
}

void
osd_finish(struct server *server)
{
	restore_preview_node(server);
	seat_focus_override_end(&server->seat);

	server->osd_state.preview_node = NULL;
	server->osd_state.preview_anchor = NULL;
	server->osd_state.cycle_view = NULL;

	destroy_osd_scenes(server);

	if (server->osd_state.preview_outline) {
		/* Destroy the whole multi_rect so we can easily react to new themes */
		wlr_scene_node_destroy(&server->osd_state.preview_outline->tree->node);
		server->osd_state.preview_outline = NULL;
	}

	/* Hiding OSD may need a cursor change */
	cursor_update_focus(server);
}

static void
preview_cycled_view(struct view *view)
{
	assert(view);
	assert(view->scene_tree);
	struct osd_state *osd_state = &view->server->osd_state;

	/* Move previous selected node back to its original place */
	restore_preview_node(view->server);

	/* Store some pointers so we can reset the preview later on */
	osd_state->preview_node = &view->scene_tree->node;
	osd_state->preview_parent = view->scene_tree->node.parent;

	/* Remember the sibling right before the selected node */
	osd_state->preview_anchor = lab_wlr_scene_get_prev_node(
		osd_state->preview_node);
	while (osd_state->preview_anchor && !osd_state->preview_anchor->data) {
		/* Ignore non-view nodes */
		osd_state->preview_anchor = lab_wlr_scene_get_prev_node(
			osd_state->preview_anchor);
	}

	/* Store node enabled / minimized state and force-enable if disabled */
	osd_state->preview_was_enabled = osd_state->preview_node->enabled;
	if (!osd_state->preview_was_enabled) {
		wlr_scene_node_set_enabled(osd_state->preview_node, true);
	}

	/*
	 * FIXME: This abuses an implementation detail of the always-on-top tree.
	 *        Create a permanent server->osd_preview_tree instead that can
	 *        also be used as parent for the preview outlines.
	 */
	wlr_scene_node_reparent(osd_state->preview_node,
		view->server->view_tree_always_on_top);

	/* Finally raise selected node to the top */
	wlr_scene_node_raise_to_top(osd_state->preview_node);
}

static void
create_osd_scene(struct output *output, struct wl_array *views)
{
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
	scaled_rect_buffer_create(output->osd_scene.tree, w, h,
		theme->osd_border_width, bg_color, theme->osd_border_color);

	int y = theme->osd_border_width + theme->osd_window_switcher_padding;

	/* Draw workspace indicator */
	if (show_workspace) {
		struct font font = rc.font_osd;
		font.weight = FONT_WEIGHT_BOLD;

		/* Center workspace indicator on the x axis */
		int x = (w - font_width(&font, workspace_name)) / 2;
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

	/* This is the width of the area available for text fields */
	int available_width = w - 2 * theme->osd_border_width
		- 2 * theme->osd_window_switcher_padding
		- 2 * theme->osd_window_switcher_item_active_border_width;

	/* Draw text for each node */
	struct view **view;
	wl_array_for_each(view, views) {
		struct osd_scene_item *item =
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

		int nr_fields = wl_list_length(&rc.window_switcher.fields);
		struct window_switcher_field *field;
		wl_list_for_each(field, &rc.window_switcher.fields, link) {
			int field_width = (available_width - (nr_fields + 1)
				* theme->osd_window_switcher_item_padding_x)
				* field->width / 100.0;
			struct wlr_scene_node *node = NULL;
			int height = -1;

			if (field->content == LAB_FIELD_ICON) {
				int icon_size = MIN(field_width,
					theme->osd_window_switcher_item_icon_size);
				struct scaled_icon_buffer *icon_buffer =
					scaled_icon_buffer_create(item_root,
						server, icon_size, icon_size);
				scaled_icon_buffer_set_app_id(icon_buffer,
					view_get_string_prop(*view, "app_id"));
				node = &icon_buffer->scene_buffer->node;
				height = icon_size;
			} else {
				buf_clear(&buf);
				osd_field_get_content(field, &buf, *view);

				struct scaled_font_buffer *font_buffer =
					scaled_font_buffer_create(item_root);
				scaled_font_buffer_update(font_buffer, buf.data, field_width,
					&rc.font_osd, text_color, bg_color);
				node = &font_buffer->scene_buffer->node;
				height = font_height(&rc.font_osd);
			}

			wlr_scene_node_set_position(node, x,
				y + (theme->osd_window_switcher_item_height - height) / 2);
			x += field_width + theme->osd_window_switcher_item_padding_x;
		}

		/* Highlight around selected window's item */
		int highlight_w = w - 2 * theme->osd_border_width
				- 2 * theme->osd_window_switcher_padding;
		int highlight_h = theme->osd_window_switcher_item_height;
		int highlight_x = theme->osd_border_width
				+ theme->osd_window_switcher_padding;
		int border_width = theme->osd_window_switcher_item_active_border_width;
		float transparent[4] = {0};

		struct scaled_rect_buffer *highlight_buffer = scaled_rect_buffer_create(
				output->osd_scene.tree, highlight_w, highlight_h,
				border_width, transparent, text_color);
		assert(highlight_buffer);
		item->highlight_outline = &highlight_buffer->scene_buffer->node;
		wlr_scene_node_set_position(item->highlight_outline, highlight_x, y);
		wlr_scene_node_set_enabled(item->highlight_outline, false);

		y += theme->osd_window_switcher_item_height;
	}
	buf_reset(&buf);

	/* Center OSD */
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	wlr_scene_node_set_position(&output->osd_scene.tree->node,
		usable.x + usable.width / 2 - w / 2,
		usable.y + usable.height / 2 - h / 2);
}

static void
update_item_highlight(struct output *output)
{
	struct osd_scene_item *item;
	wl_array_for_each(item, &output->osd_scene.items) {
		wlr_scene_node_set_enabled(item->highlight_outline,
			item->view == output->server->osd_state.cycle_view);
	}
}

static void
update_osd(struct server *server)
{
	struct wl_array views;
	wl_array_init(&views);
	view_array_append(server, &views, rc.window_switcher.criteria);

	if (!wl_array_len(&views) || !server->osd_state.cycle_view) {
		osd_finish(server);
		goto out;
	}

	if (rc.window_switcher.show && rc.theme->osd_window_switcher_width > 0) {
		/* Display the actual OSD */
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
			if (!output_is_usable(output)) {
				continue;
			}
			if (!output->osd_scene.tree) {
				create_osd_scene(output, &views);
				assert(output->osd_scene.tree);
			}
			update_item_highlight(output);
		}
	}

	/* Outline current window */
	if (rc.window_switcher.outlines) {
		if (view_is_focusable(server->osd_state.cycle_view)) {
			osd_update_preview_outlines(server->osd_state.cycle_view);
		}
	}

	if (rc.window_switcher.preview) {
		preview_cycled_view(server->osd_state.cycle_view);
	}
out:
	wl_array_release(&views);
}
