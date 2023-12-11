// SPDX-License-Identifier: GPL-2.0-only
#include "config.h"
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <pango/pangocairo.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>
#include "buffer.h"
#include "common/array.h"
#include "common/buf.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "theme.h"
#include "node.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"

/* is title different from app_id/class? */
static int
is_title_different(struct view *view)
{
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		return g_strcmp0(view_get_string_prop(view, "title"),
			view_get_string_prop(view, "app_id"));
#if HAVE_XWAYLAND
	case LAB_XWAYLAND_VIEW:
		return g_strcmp0(view_get_string_prop(view, "title"),
			view_get_string_prop(view, "class"));
#endif
	}
	return 1;
}

static const char *
get_formatted_app_id(struct view *view)
{
	char *s = (char *)view_get_string_prop(view, "app_id");
	if (!s) {
		return NULL;
	}
	if (rc.window_switcher.full_app_id) {
		return s;
	}
	/* remove the first two nodes of 'org.' strings */
	if (!strncmp(s, "org.", 4)) {
		char *p = s + 4;
		p = strchr(p, '.');
		if (p) {
			return ++p;
		}
	}
	return s;
}

static void
destroy_osd_nodes(struct output *output)
{
	struct wlr_scene_node *child, *next;
	struct wl_list *children = &output->osd_tree->children;
	wl_list_for_each_safe(child, next, children, link) {
		wlr_scene_node_destroy(child);
	}
}

static void
osd_update_preview_outlines(struct view *view)
{
	/* Create / Update preview outline tree */
	struct server *server = view->server;
	struct multi_rect *rect = view->server->osd_state.preview_outline;
	if (!rect) {
		int line_width = server->theme->osd_border_width;
		float *colors[] = {
			server->theme->osd_bg_color,
			server->theme->osd_label_text_color,
			server->theme->osd_bg_color
		};
		rect = multi_rect_create(&server->scene->tree, colors, line_width);
		wlr_scene_node_place_above(&rect->tree->node, &server->menu_tree->node);
		server->osd_state.preview_outline = rect;
	}

	struct wlr_box geo = ssd_max_extents(view);
	multi_rect_set_size(rect, geo.width, geo.height);
	wlr_scene_node_set_position(&rect->tree->node, geo.x, geo.y);
}

void
osd_on_view_destroy(struct view *view)
{
	assert(view);
	struct osd_state *osd_state = &view->server->osd_state;

	if (!osd_state->cycle_view) {
		/* OSD not active, no need for clean up */
		return;
	}

	if (osd_state->cycle_view == view) {
		/*
		 * If we are the current OSD selected view, cycle
		 * to the next because we are dying.
		 */

		/* Also resets preview node */
		osd_state->cycle_view = desktop_cycle_view(view->server,
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
		/* Update the OSD to reflect the view has now gone. */
		osd_update(view->server);
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

void
osd_finish(struct server *server)
{
	server->osd_state.preview_node = NULL;
	server->osd_state.preview_anchor = NULL;

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		destroy_osd_nodes(output);
		wlr_scene_node_set_enabled(&output->osd_tree->node, false);
	}
	if (server->osd_state.preview_outline) {
		/* Destroy the whole multi_rect so we can easily react to new themes */
		wlr_scene_node_destroy(&server->osd_state.preview_outline->tree->node);
		server->osd_state.preview_outline = NULL;
	}

	/* Hiding OSD may need a cursor change */
	cursor_update_focus(server);

	/*
	 * We delay resetting cycle_view until after cursor_update_focus()
	 * has been called to allow A-Tab keyboard focus switching even if
	 * followMouse has been configured and the cursor is on a different
	 * surface. Otherwise cursor_update_focus() would automatically
	 * refocus the surface the cursor is currently on.
	 */
	server->osd_state.cycle_view = NULL;
}

void
osd_preview_restore(struct server *server)
{
	struct osd_state *osd_state = &server->osd_state;
	if (osd_state->preview_node) {
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
		osd_state->preview_anchor = NULL;
	}
}

static void
preview_cycled_view(struct view *view)
{
	assert(view);
	assert(view->scene_tree);
	struct osd_state *osd_state = &view->server->osd_state;

	/* Move previous selected node back to its original place */
	osd_preview_restore(view->server);

	/* Remember the sibling right before the selected node */
	osd_state->preview_node = &view->scene_tree->node;
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

	/* Finally raise selected node to the top */
	wlr_scene_node_raise_to_top(osd_state->preview_node);
}

static const char *
get_type(struct view *view)
{
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		return "[xdg-shell]";
#if HAVE_XWAYLAND
	case LAB_XWAYLAND_VIEW:
		return "[xwayland]";
#endif
	}
	return "";
}

static const char *
get_app_id(struct view *view)
{
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		return get_formatted_app_id(view);
#if HAVE_XWAYLAND
	case LAB_XWAYLAND_VIEW:
		return view_get_string_prop(view, "class");
#endif
	}
	return "";
}

static const char *
get_title(struct view *view)
{
	if (is_title_different(view)) {
		return view_get_string_prop(view, "title");
	} else {
		return "";
	}
}

static void
render_osd(struct server *server, cairo_t *cairo, int w, int h,
		struct wl_list *node_list, bool show_workspace,
		const char *workspace_name, struct wl_array *views)
{
	struct view *cycle_view = server->osd_state.cycle_view;
	struct theme *theme = server->theme;

	cairo_surface_t *surf = cairo_get_target(cairo);

	/* Draw background */
	set_cairo_color(cairo, theme->osd_bg_color);
	cairo_rectangle(cairo, 0, 0, w, h);
	cairo_fill(cairo);

	/* Draw border */
	set_cairo_color(cairo, theme->osd_border_color);
	struct wlr_fbox fbox = {
		.width = w,
		.height = h,
	};
	draw_cairo_border(cairo, fbox, theme->osd_border_width);

	/* Set up text rendering */
	set_cairo_color(cairo, theme->osd_label_text_color);
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

	PangoFontDescription *desc = font_to_pango_desc(&rc.font_osd);
	pango_layout_set_font_description(layout, desc);

	pango_cairo_update_layout(cairo, layout);

	int y = theme->osd_border_width + theme->osd_window_switcher_padding;

	/* Draw workspace indicator */
	if (show_workspace) {
		/* Center workspace indicator on the x axis */
		int x = font_width(&rc.font_osd, workspace_name);
		x = (theme->osd_window_switcher_width - x) / 2;
		cairo_move_to(cairo, x, y + theme->osd_window_switcher_item_active_border_width);
		PangoWeight weight = pango_font_description_get_weight(desc);
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout, workspace_name, -1);
		pango_cairo_show_layout(cairo, layout);
		pango_font_description_set_weight(desc, weight);
		pango_layout_set_font_description(layout, desc);
		y += theme->osd_window_switcher_item_height;
	}
	pango_font_description_free(desc);

	struct buf buf;
	buf_init(&buf);

	/* This is the width of the area available for text fields */
	int available_width = w - 2 * theme->osd_border_width
		- 2 * theme->osd_window_switcher_padding
		- 2 * theme->osd_window_switcher_item_active_border_width;

	/* Draw text for each node */
	struct view **view;
	wl_array_for_each(view, views) {
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

		int nr_fields = wl_list_length(&rc.window_switcher.fields);
		struct window_switcher_field *field;
		wl_list_for_each(field, &rc.window_switcher.fields, link) {
			buf.len = 0;
			cairo_move_to(cairo, x, y
				+ theme->osd_window_switcher_item_padding_y
				+ theme->osd_window_switcher_item_active_border_width);

			switch (field->content) {
			case LAB_FIELD_TYPE:
				buf_add(&buf, get_type(*view));
				break;
			case LAB_FIELD_IDENTIFIER:
				buf_add(&buf, get_app_id(*view));
				break;
			case LAB_FIELD_TITLE:
				buf_add(&buf, get_title(*view));
				break;
			default:
				break;
			}
			int field_width = (available_width - (nr_fields + 1)
				* theme->osd_window_switcher_item_padding_x)
				* field->width / 100.0;
			pango_layout_set_width(layout, field_width * PANGO_SCALE);
			pango_layout_set_text(layout, buf.buf, -1);
			pango_cairo_show_layout(cairo, layout);
			x += field_width + theme->osd_window_switcher_item_padding_x;
		}

		if (*view == cycle_view) {
			/* Highlight current window */
			struct wlr_fbox fbox = {
				.x = theme->osd_border_width + theme->osd_window_switcher_padding,
				.y = y,
				.width = theme->osd_window_switcher_width
					- 2 * theme->osd_border_width
					- 2 * theme->osd_window_switcher_padding,
				.height = theme->osd_window_switcher_item_height,
			};
			draw_cairo_border(cairo, fbox,
				theme->osd_window_switcher_item_active_border_width);
			cairo_stroke(cairo);
		}

		y += theme->osd_window_switcher_item_height;
	}
	free(buf.buf);
	g_object_unref(layout);

	cairo_surface_flush(surf);
}

static void
display_osd(struct output *output)
{
	struct server *server = output->server;
	struct theme *theme = server->theme;
	struct wl_list *node_list =
		&server->workspace_current->tree->children;
	bool show_workspace = wl_list_length(&rc.workspace_config.workspaces) > 1;
	const char *workspace_name = server->workspace_current->name;

	struct wl_array views;
	wl_array_init(&views);
	view_array_append(server, &views,
		LAB_VIEW_CRITERIA_CURRENT_WORKSPACE
		| LAB_VIEW_CRITERIA_NO_ALWAYS_ON_TOP
		| LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER);

	float scale = output->wlr_output->scale;
	int w = theme->osd_window_switcher_width;
	int h = wl_array_len(&views) * rc.theme->osd_window_switcher_item_height
		+ 2 * rc.theme->osd_border_width
		+ 2 * rc.theme->osd_window_switcher_padding;
	if (show_workspace) {
		/* workspace indicator */
		h += theme->osd_window_switcher_item_height;
	}

	/* Reset buffer */
	if (output->osd_buffer) {
		wlr_buffer_drop(&output->osd_buffer->base);
	}
	output->osd_buffer = buffer_create_cairo(w, h, scale, true);
	if (!output->osd_buffer) {
		wlr_log(WLR_ERROR, "Failed to allocate cairo buffer for the window switcher");
		return;
	}

	/* Render OSD image */
	cairo_t *cairo = output->osd_buffer->cairo;
	render_osd(server, cairo, w, h, node_list, show_workspace,
		workspace_name, &views);
	wl_array_release(&views);

	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_create(
		output->osd_tree, &output->osd_buffer->base);
	wlr_scene_buffer_set_dest_size(scene_buffer, w, h);

	/* Center OSD */
	struct wlr_box output_box;
	wlr_output_layout_get_box(output->server->output_layout,
		output->wlr_output, &output_box);
	int lx = output->usable_area.x + output->usable_area.width / 2
		- w / 2 + output_box.x;
	int ly = output->usable_area.y + output->usable_area.height / 2
		- h / 2 + output_box.y;
	wlr_scene_node_set_position(&scene_buffer->node, lx, ly);
	wlr_scene_node_set_enabled(&output->osd_tree->node, true);

	/* Update cursor, in case it is within the area covered by OSD */
	cursor_update_focus(server);
}

void
osd_update(struct server *server)
{
	struct wl_list *node_list =
		&server->workspace_current->tree->children;

	if (wl_list_empty(node_list) || !server->osd_state.cycle_view) {
		osd_finish(server);
		return;
	}

	if (rc.window_switcher.show && rc.theme->osd_window_switcher_width > 0) {
		/* Display the actual OSD */
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
			destroy_osd_nodes(output);
			if (output_is_usable(output)) {
				display_osd(output);
			}
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
}
