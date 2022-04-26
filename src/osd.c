// SPDX-License-Identifier: GPL-2.0-only
#include "config.h"
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <pango/pangocairo.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/buf.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "theme.h"
#include "node.h"
#include "workspaces.h"

#define OSD_ITEM_HEIGHT (20)
#define OSD_ITEM_WIDTH (600)
#define OSD_ITEM_PADDING (10)
#define OSD_BORDER_WIDTH (6)
#define OSD_TAB1 (120)
#define OSD_TAB2 (300)

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
			view->xwayland_surface->class);
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

static int
get_osd_height(struct wl_list *node_list)
{
	int height = 0;
	struct view *view;
	struct wlr_scene_node *node;
	wl_list_for_each(node, node_list, link) {
		view = node_view_from_node(node);
		if (!isfocusable(view)) {
			continue;
		}
		height += OSD_ITEM_HEIGHT;
	}
	height += 2 * OSD_BORDER_WIDTH;
	return height;
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
	struct multi_rect *rect = view->server->osd_preview_outline;
	if (!rect) {
		int line_width = server->theme->osd_border_width;
		float *colors[] = {
			server->theme->osd_bg_color,
			server->theme->osd_label_text_color,
			server->theme->osd_bg_color
		};
		rect = multi_rect_create(&server->scene->tree, colors, line_width);
		wlr_scene_node_place_above(&rect->tree->node, &server->menu_tree->node);
		server->osd_preview_outline = rect;
	}

	struct wlr_box geo = ssd_max_extents(view);
	multi_rect_set_size(rect, geo.width, geo.height);
	wlr_scene_node_set_position(&rect->tree->node, geo.x, geo.y);
}

void
osd_finish(struct server *server)
{
	server->osd_state.cycle_view = NULL;
	server->osd_state.preview_node = NULL;
	server->osd_state.preview_anchor = NULL;

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		destroy_osd_nodes(output);
		wlr_scene_node_set_enabled(&output->osd_tree->node, false);
	}
	if (server->osd_preview_outline) {
		/* Destroy the whole multi_rect so we can easily react to new themes */
		wlr_scene_node_destroy(&server->osd_preview_outline->tree->node);
		server->osd_preview_outline = NULL;
	}
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

	/* Store node enabled / minimized state and force-enable if disabled */
	osd_state->preview_was_enabled = osd_state->preview_node->enabled;
	if (!osd_state->preview_was_enabled) {
		wlr_scene_node_set_enabled(osd_state->preview_node, true);
	}

	/* Finally raise selected node to the top */
	wlr_scene_node_raise_to_top(osd_state->preview_node);
}

void
osd_update(struct server *server)
{
	struct wl_list *node_list =
		&server->workspace_current->tree->children;

	if (wl_list_empty(node_list)) {
		osd_finish(server);
		return;
	}

	struct theme *theme = server->theme;
	bool show_workspace = wl_list_length(&rc.workspace_config.workspaces) > 1;

	struct buf buf;
	buf_init(&buf);

	struct view *view;
	struct output *output;
	struct wlr_scene_node *node;
	wl_list_for_each(output, &server->outputs, link) {
		destroy_osd_nodes(output);
		if (!output->wlr_output->enabled) {
			continue;
		}

		float scale = output->wlr_output->scale;
		int w = OSD_ITEM_WIDTH + (2 * OSD_BORDER_WIDTH);
		int h = get_osd_height(node_list);
		if (show_workspace) {
			/* workspace indicator */
			h += OSD_ITEM_HEIGHT;
		}

		if (output->osd_buffer) {
			wlr_buffer_drop(&output->osd_buffer->base);
		}
		output->osd_buffer = buffer_create_cairo(w, h, scale, true);

		cairo_t *cairo = output->osd_buffer->cairo;
		cairo_surface_t *surf = cairo_get_target(cairo);

		/* background */
		set_cairo_color(cairo, theme->osd_bg_color);
		cairo_rectangle(cairo, 0, 0, w, h);
		cairo_fill(cairo);

		/* Border */
		set_cairo_color(cairo, theme->osd_border_color);
		draw_cairo_border(cairo, w, h, theme->osd_border_width);

		int y = OSD_BORDER_WIDTH;

		if (show_workspace) {
			/* workspace indicator */
			y += OSD_ITEM_HEIGHT;
		}

		/* highlight current window */
		wl_list_for_each_reverse(node, node_list, link) {
			view = node_view_from_node(node);
			if (!isfocusable(view)) {
				continue;
			}
			if (view == server->osd_state.cycle_view) {
				set_cairo_color(cairo, theme->osd_label_text_color);
				cairo_rectangle(cairo, OSD_BORDER_WIDTH, y,
					OSD_ITEM_WIDTH, OSD_ITEM_HEIGHT);
				cairo_stroke(cairo);
				if (rc.cycle_preview_outlines) {
					osd_update_preview_outlines(view);
				}
				break;
			}
			y += OSD_ITEM_HEIGHT;
		}

		/* text */
		set_cairo_color(cairo, theme->osd_label_text_color);
		PangoLayout *layout = pango_cairo_create_layout(cairo);
		pango_layout_set_width(layout,
			(OSD_ITEM_WIDTH - 2 * OSD_ITEM_PADDING) * PANGO_SCALE);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

		struct font font = {
			.name = rc.font_name_osd,
			.size = rc.font_size_osd,
		};
		PangoFontDescription *desc = pango_font_description_new();
		pango_font_description_set_family(desc, font.name);
		pango_font_description_set_size(desc, font.size * PANGO_SCALE);
		pango_layout_set_font_description(layout, desc);

		PangoTabArray *tabs = pango_tab_array_new_with_positions(2, TRUE,
			PANGO_TAB_LEFT, OSD_TAB1, PANGO_TAB_LEFT, OSD_TAB2);
		pango_layout_set_tabs(layout, tabs);
		pango_tab_array_free(tabs);

		pango_cairo_update_layout(cairo, layout);

		y = OSD_BORDER_WIDTH;

		/* Center text entries on the y axis */
		int y_offset = (OSD_ITEM_HEIGHT - font_height(&font)) / 2;
		y += y_offset;

		if (show_workspace) {
			/* Center workspace indicator on the x axis */
			int x = font_width(&font, server->workspace_current->name);
			x = (OSD_ITEM_WIDTH - x) / 2;
			cairo_move_to(cairo, x, y);
			pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
			pango_layout_set_font_description(layout, desc);
			pango_layout_set_text(layout, server->workspace_current->name, -1);
			pango_cairo_show_layout(cairo, layout);
			pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
			pango_layout_set_font_description(layout, desc);
			y += OSD_ITEM_HEIGHT;
		}
		pango_font_description_free(desc);

		wl_list_for_each_reverse(node, node_list, link) {
			view = node_view_from_node(node);
			if (!isfocusable(view)) {
				continue;
			}
			buf.len = 0;
			cairo_move_to(cairo, OSD_BORDER_WIDTH + OSD_ITEM_PADDING, y);

			switch (view->type) {
			case LAB_XDG_SHELL_VIEW:
				buf_add(&buf, "[xdg-shell]\t");
				buf_add(&buf, get_formatted_app_id(view));
				buf_add(&buf, "\t");
				break;
#if HAVE_XWAYLAND
			case LAB_XWAYLAND_VIEW:
				buf_add(&buf, "[xwayland]\t");
				buf_add(&buf, view_get_string_prop(view, "class"));
				buf_add(&buf, "\t");
				break;
#endif
			}

			if (is_title_different(view)) {
				buf_add(&buf, view_get_string_prop(view, "title"));
			}

			pango_layout_set_text(layout, buf.buf, -1);
			pango_cairo_show_layout(cairo, layout);
			y += OSD_ITEM_HEIGHT;
		}
		g_object_unref(layout);
		cairo_surface_flush(surf);

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
	}
	free(buf.buf);

	if (rc.cycle_preview_contents) {
		preview_cycled_view(server->osd_state.cycle_view);
	}
}
