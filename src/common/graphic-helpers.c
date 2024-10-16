// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include "buffer.h"
#include "common/graphic-helpers.h"
#include "common/macros.h"
#include "common/mem.h"

static void
border_rect_destroy_notify(struct wl_listener *listener, void *data)
{
	struct border_rect *rect = wl_container_of(listener, rect, destroy);
	wl_list_remove(&rect->destroy.link);
	free(rect);
}

struct border_rect *
border_rect_create(struct wlr_scene_tree *parent, float border_color[4],
		float bg_color[4], int border_width)
{
	struct border_rect *rect = znew(*rect);
	rect->border_width = border_width;
	rect->tree = wlr_scene_tree_create(parent);
	rect->destroy.notify = border_rect_destroy_notify;
	wl_signal_add(&rect->tree->node.events.destroy, &rect->destroy);

	rect->top = wlr_scene_rect_create(rect->tree, 0, 0, border_color);
	rect->right = wlr_scene_rect_create(rect->tree, 0, 0, border_color);
	rect->bottom = wlr_scene_rect_create(rect->tree, 0, 0, border_color);
	rect->left = wlr_scene_rect_create(rect->tree, 0, 0, border_color);
	if (bg_color) {
		rect->bg = wlr_scene_rect_create(rect->tree, 0, 0, bg_color);
	}

	return rect;
}

void
border_rect_set_size(struct border_rect *rect, int width, int height)
{
	assert(rect);
	int border_width = rect->border_width;

	/*
	 * The border is drawn like below:
	 *
	 * <--width-->
	 * +---------+   ^
	 * +-+-----+-+   |
	 * | |     | | height
	 * | |     | |   |
	 * +-+-----+-+   |
	 * +---------+   v
	 */

	/* Update positions (positions of top/left rectangles are static) */
	wlr_scene_node_set_position(&rect->top->node,
		0, 0);
	wlr_scene_node_set_position(&rect->bottom->node,
		0, height - border_width);
	wlr_scene_node_set_position(&rect->left->node,
		0, border_width);
	wlr_scene_node_set_position(&rect->right->node,
		width - border_width, border_width);
	if (rect->bg) {
		wlr_scene_node_set_position(&rect->bg->node,
			border_width, border_width);
	}

	/* Update sizes */
	wlr_scene_rect_set_size(rect->top,
		width, border_width);
	wlr_scene_rect_set_size(rect->bottom,
		width, border_width);
	wlr_scene_rect_set_size(rect->left,
		border_width, height - border_width * 2);
	wlr_scene_rect_set_size(rect->right,
		border_width, height - border_width * 2);
	if (rect->bg) {
		wlr_scene_rect_set_size(rect->bg,
			width - border_width * 2,
			height - border_width * 2);
	}
}

static void
multi_rect_destroy_notify(struct wl_listener *listener, void *data)
{
	struct multi_rect *rect = wl_container_of(listener, rect, destroy);
	wl_list_remove(&rect->destroy.link);
	free(rect);
}

struct multi_rect *
multi_rect_create(struct wlr_scene_tree *parent, float *colors[3], int line_width)
{
	struct multi_rect *rect = znew(*rect);
	rect->tree = wlr_scene_tree_create(parent);
	rect->destroy.notify = multi_rect_destroy_notify;
	wl_signal_add(&rect->tree->node.events.destroy, &rect->destroy);
	for (size_t i = 0; i < 3; i++) {
		rect->border_rects[i] = border_rect_create(rect->tree, colors[i],
			NULL, line_width);
		wlr_scene_node_set_position(&rect->border_rects[i]->tree->node,
			i * line_width, i * line_width);
	}
	return rect;
}

void
multi_rect_set_size(struct multi_rect *multi_rect, int width, int height)
{
	assert(multi_rect);
	int line_width = multi_rect->border_rects[0]->border_width;
	for (size_t i = 0; i < 3; i++) {
		border_rect_set_size(multi_rect->border_rects[i],
			width - i * line_width * 2, height - i * line_width * 2);
	}
}

/* Draws a border with a specified line width */
void
draw_cairo_border(cairo_t *cairo, struct wlr_fbox fbox, double line_width)
{
	cairo_save(cairo);

	/* The anchor point of a line is in the center */
	fbox.x += line_width / 2.0;
	fbox.y += line_width / 2.0;
	fbox.width -= line_width;
	fbox.height -= line_width;
	cairo_set_line_width(cairo, line_width);
	cairo_rectangle(cairo, fbox.x, fbox.y, fbox.width, fbox.height);
	cairo_stroke(cairo);

	cairo_restore(cairo);
}

/* Sets the cairo color. Splits the single color channels */
void
set_cairo_color(cairo_t *cairo, const float *c)
{
	/*
	 * We are dealing with pre-multiplied colors
	 * but cairo expects unmultiplied colors here
	 */
	float alpha = c[3];

	if (alpha == 0.0f) {
		cairo_set_source_rgba(cairo, 0, 0, 0, 0);
		return;
	}

	cairo_set_source_rgba(cairo, c[0] / alpha, c[1] / alpha,
		c[2] / alpha, alpha);
}
