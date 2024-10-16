/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_GRAPHIC_HELPERS_H
#define LABWC_GRAPHIC_HELPERS_H

#include <cairo.h>
#include <wayland-server-core.h>

struct wlr_scene_tree;
struct wlr_scene_rect;
struct wlr_fbox;

struct border_rect {
	struct wlr_scene_tree *tree;
	int border_width; /* read-only */

	/* Private */
	struct wlr_scene_rect *top;
	struct wlr_scene_rect *bottom;
	struct wlr_scene_rect *left;
	struct wlr_scene_rect *right;
	struct wlr_scene_rect *bg;

	struct wl_listener destroy;
};

struct multi_rect {
	struct wlr_scene_tree *tree;

	/* Private */
	struct border_rect *border_rects[3];
	struct wl_listener destroy;
};

/**
 * Create a new rectangular with a single border (border_rect).
 *
 * bg_color can take NULL, in which case no background is rendered.
 *
 * A border_rect can be positioned by positioning border_rect->tree->node.
 *
 * It can be destroyed by destroying its tree node (or one of its parent nodes).
 * Once the tree node has been destroyed the struct will be free'd automatically.
 */
struct border_rect *border_rect_create(struct wlr_scene_tree *parent,
	float border_color[4], float bg_color[4], int border_width);

void border_rect_set_size(struct border_rect *rect, int width, int height);

/**
 * Create a new multi_rect.
 *
 * A multi_rect consists of 3 nested border_rects without a background.
 * Each of the border_rect is using the same @line_width but its own color
 * based on the @colors argument.
 */
struct multi_rect *multi_rect_create(struct wlr_scene_tree *parent,
		float *colors[3], int line_width);

void multi_rect_set_size(struct multi_rect *rect, int width, int height);

/**
 * Sets the cairo color.
 * Splits a float[4] single color array into its own arguments
 */
void set_cairo_color(cairo_t *cairo, const float *color);

/* Draws a border with a specified line width */
void draw_cairo_border(cairo_t *cairo, struct wlr_fbox fbox, double line_width);

struct lab_data_buffer;

#endif /* LABWC_GRAPHIC_HELPERS_H */
