/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_GRAPHIC_HELPERS_H
#define LABWC_GRAPHIC_HELPERS_H

#include <cairo.h>
#include <wayland-server-core.h>

struct wlr_scene_tree;
struct wlr_scene_rect;
struct wlr_fbox;

struct outline_rect {
	struct wlr_scene_tree *tree;
	int line_width; /* read-only */

	/* Private */
	struct wlr_scene_rect *top;
	struct wlr_scene_rect *bottom;
	struct wlr_scene_rect *left;
	struct wlr_scene_rect *right;

	struct wl_listener destroy;
};

struct multi_rect {
	struct wlr_scene_tree *tree;

	/* Private */
	struct outline_rect *outlines[3];
	struct wl_listener destroy;
};

/**
 * Create a new rectangular outline (outline_rect).
 *
 * An outline_rect can be positioned by positioning outline_rect->tree->node.
 *
 * It can be destroyed by destroying its tree node (or one of its parent nodes).
 * Once the tree node has been destroyed the struct will be free'd automatically.
 */
struct outline_rect *outline_rect_create(struct wlr_scene_tree *parent,
		float color[4], int line_width);

void outline_rect_set_size(struct outline_rect *rect, int width, int height);

/**
 * Create a new multi_rect.
 *
 * A multi_rect consists of 3 nested outline_rects.
 * Each of the outline_rect is using the same @line_width but its own color
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

struct surface_context {
	bool is_duplicate;
	cairo_surface_t *surface;
};

struct surface_context get_cairo_surface_from_lab_data_buffer(
	struct lab_data_buffer *buffer);

#endif /* LABWC_GRAPHIC_HELPERS_H */
