/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_GRAPHIC_HELPERS_H
#define LABWC_GRAPHIC_HELPERS_H

#include <stdbool.h>
#include <stdint.h>
#include <cairo.h>

struct wlr_fbox;
struct wlr_scene_buffer;
struct wlr_scene_node;
struct wlr_scene_tree;

/**
 * Sets the cairo color.
 * Splits a float[4] single color array into its own arguments
 */
void set_cairo_color(cairo_t *cairo, const float *color);

/* Creates a solid color cairo pattern from premultipled RGBA */
cairo_pattern_t *color_to_pattern(const float *color);

bool is_pattern_opaque(cairo_pattern_t *pattern);

/* Like zfree() but for a cairo_pattern_t */
#define zfree_pattern(ptr) do { \
	if (ptr) { \
		cairo_pattern_destroy(ptr); \
		(ptr) = NULL; \
	} \
} while (0)

/* Draws a border with a specified line width */
void draw_cairo_border(cairo_t *cairo, struct wlr_fbox fbox, double line_width);

/* Converts X11 color name to ARGB32 (with alpha = 255) */
bool lookup_named_color(const char *name, uint32_t *argb);

/**
 * create_rounded_rect_bg - Draw a filled rounded rectangle background.
 *
 * All four corners are rounded by @corner_radius. Falls back to a
 * plain rectangular scene rect if Cairo buffer creation fails.
 *
 * @parent: parent scene tree node
 * @width: width in layout pixels
 * @height: height in layout pixels
 * @color: premultiplied RGBA fill color (float[4])
 * @corner_radius: corner radius in layout pixels
 * @lower_to_bottom: if true, background is lowered to bottom
 */
void create_rounded_rect_bg(struct wlr_scene_tree *parent, int width,
	int height, const float color[4], int corner_radius,
	bool lower_to_bottom);

#endif /* LABWC_GRAPHIC_HELPERS_H */
