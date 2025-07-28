/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_GRAPHIC_HELPERS_H
#define LABWC_GRAPHIC_HELPERS_H

#include <stdbool.h>
#include <stdint.h>
#include <cairo.h>

struct wlr_fbox;

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

#endif /* LABWC_GRAPHIC_HELPERS_H */
