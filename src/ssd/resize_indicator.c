// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/scaled_font_buffer.h"
#include "labwc.h"
#include "resize_indicator.h"
#include "view.h"

static void
resize_indicator_reconfigure_view(struct resize_indicator *indicator)
{
	assert(indicator->tree);

	struct theme *theme = rc.theme;
	indicator->height = font_height(&rc.font_osd)
		+ 2 * theme->osd_window_switcher_padding
		+ 2 * theme->osd_border_width;

	/* Static positions */
	wlr_scene_node_set_position(&indicator->background->node,
		theme->osd_border_width, theme->osd_border_width);

	wlr_scene_node_set_position(&indicator->text->scene_buffer->node,
		theme->osd_border_width + theme->osd_window_switcher_padding,
		theme->osd_border_width + theme->osd_window_switcher_padding);

	/* Colors */
	wlr_scene_rect_set_color(indicator->border, theme->osd_border_color);
	wlr_scene_rect_set_color(indicator->background, theme->osd_bg_color);
}

static void
resize_indicator_init(struct view *view)
{
	assert(view);
	struct resize_indicator *indicator = &view->resize_indicator;
	assert(!indicator->tree);

	indicator->tree = wlr_scene_tree_create(view->scene_tree);
	indicator->border = wlr_scene_rect_create(
		indicator->tree, 0, 0, rc.theme->osd_border_color);
	indicator->background = wlr_scene_rect_create(
		indicator->tree, 0, 0, rc.theme->osd_bg_color);
	indicator->text = scaled_font_buffer_create(indicator->tree);

	wlr_scene_node_set_enabled(&indicator->tree->node, false);
	resize_indicator_reconfigure_view(indicator);
}

static bool
wants_indicator(struct view *view)
{
	assert(view);

	if (rc.resize_indicator == LAB_RESIZE_INDICATOR_NON_PIXEL) {
		if (view->server->input_mode != LAB_INPUT_STATE_RESIZE) {
			return false;
		}
		struct view_size_hints hints = view_get_size_hints(view);
		if (hints.width_inc && hints.height_inc) {
			return true;
		}
	}
	return rc.resize_indicator == LAB_RESIZE_INDICATOR_ALWAYS;
}

void
resize_indicator_reconfigure(struct server *server)
{
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		struct resize_indicator *indicator = &view->resize_indicator;
		if (indicator->tree) {
			resize_indicator_reconfigure_view(indicator);
		}
		if (view != server->grabbed_view) {
			continue;
		}

		/* This view is currently in an interactive move/resize operation */
		if (indicator->tree && indicator->tree->node.enabled) {
			/* Indicator was active while reloading the config */
			if (wants_indicator(view)) {
				/* Apply new font setting */
				resize_indicator_update(view);
			} else {
				/* Indicator was disabled in config */
				resize_indicator_hide(view);
			}
		} else if (wants_indicator(view)) {
			/* Indicator not yet active */
			resize_indicator_show(view);
		}
	}
}

static void
resize_indicator_set_size(struct resize_indicator *indicator, int width)
{
	assert(indicator->tree);

	/* We are not using a width-cache-early-out here to allow for theme changes */
	indicator->width = width
		+ 2 * rc.theme->osd_window_switcher_padding
		+ 2 * rc.theme->osd_border_width;

	wlr_scene_rect_set_size(indicator->border, indicator->width, indicator->height);
	wlr_scene_rect_set_size(indicator->background,
		indicator->width - 2 * rc.theme->osd_border_width,
		indicator->height - 2 * rc.theme->osd_border_width);
}

void
resize_indicator_show(struct view *view)
{
	assert(view);

	if (!wants_indicator(view)) {
		return;
	}

	struct resize_indicator *indicator = &view->resize_indicator;
	if (!indicator->tree) {
		/* Lazy initialize */
		resize_indicator_init(view);
	}

	wlr_scene_node_raise_to_top(&indicator->tree->node);
	wlr_scene_node_set_enabled(&indicator->tree->node, true);
	resize_indicator_update(view);
}

void
resize_indicator_update(struct view *view)
{
	assert(view);
	assert(view == view->server->grabbed_view);

	if (!wants_indicator(view)) {
		return;
	}

	struct resize_indicator *indicator = &view->resize_indicator;
	if (!indicator->tree) {
		/*
		 * Future-proofs this routine:
		 *
		 * This can only happen when either src/interactive.c
		 * stops calling resize_indicator_show(), there is a
		 * bug in this file or resize_indicator_reconfigure()
		 * gets changed.
		 */
		wlr_log(WLR_INFO, "Warning: resize_indicator has to use a fallback path");
		resize_indicator_show(view);
	}

	char text[32]; /* 12345 x 12345 would be 13 chars + 1 null byte */

	int eff_height = view_effective_height(view, /* use_pending */ false);
	int eff_width = view->current.width;

	switch (view->server->input_mode) {
	case LAB_INPUT_STATE_RESIZE:
		; /* works around "a label can only be part of a statement" */
		struct view_size_hints hints = view_get_size_hints(view);
		snprintf(text, sizeof(text), "%d x %d",
			MAX(0, eff_width - hints.base_width)
				/ MAX(1, hints.width_inc),
			MAX(0, eff_height - hints.base_height)
				/ MAX(1, hints.height_inc));
		break;
	case LAB_INPUT_STATE_MOVE:
		; /* works around "a label can only be part of a statement" */
		struct border margin = ssd_get_margin(view->ssd);
		snprintf(text, sizeof(text), "%d , %d",
			view->current.x - margin.left,
			view->current.y - margin.top);
		break;
	default:
		wlr_log(WLR_ERROR, "Invalid input mode for indicator update %u",
			view->server->input_mode);
		return;
	}

	/* Let the indicator change width as required by the content */
	int width = font_width(&rc.font_osd, text);

	/* font_extents() adds 4 pixels to the calculated width */
	width -= 4;

	resize_indicator_set_size(indicator, width);

	/* Center the indicator in the window */
	wlr_scene_node_set_position(&indicator->tree->node,
		(eff_width - indicator->width) / 2,
		(eff_height - indicator->height) / 2);

	scaled_font_buffer_update(indicator->text, text, width, &rc.font_osd,
		rc.theme->osd_label_text_color, rc.theme->osd_bg_color,
		NULL /* const char *arrow */);
}

void
resize_indicator_hide(struct view *view)
{
	assert(view);

	struct resize_indicator *indicator = &view->resize_indicator;
	if (!indicator->tree) {
		return;
	}

	wlr_scene_node_set_enabled(&indicator->tree->node, false);
}
