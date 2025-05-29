/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_FONT_H
#define LABWC_FONT_H

#include <cairo.h>
#include <pango/pango-font.h>

struct lab_data_buffer;

struct font {
	char *name;
	int size;
	PangoStyle slant;
	PangoWeight weight;
};

struct _PangoFontDescription *font_to_pango_desc(struct font *font);

/**
 * font_height - get font vertical extents
 * @font: description of font including family name and size
 */
int font_height(struct font *font);

/**
 * font_width - get font horizontal extents
 * @font: description of font including family name and size
 */
int font_width(struct font *font, const char *string);

/**
 * font_get_buffer_size - dry-run font_buffer_create() to get buffer size
 */
void font_get_buffer_size(int max_width, const char *text, struct font *font,
	int *width, int *height);

/**
 * font_buffer_create - Create ARGB8888 lab_data_buffer using pango
 * @buffer: buffer pointer
 * @max_width: max allowable width; will be ellipsized if longer
 * @height: buffer height or -1 to compute from font
 * @text: text to be generated as texture
 * @font: font description
 * @color: foreground color in rgba format
 * @bg_pattern: background pattern
 */
void font_buffer_create(struct lab_data_buffer **buffer, int max_width,
	int height, const char *text, struct font *font, const float *color,
	cairo_pattern_t *bg_pattern, double scale);

/**
 * font_finish - free some font related resources
 * Note: use on exit
 */
void font_finish(void);

#endif /* LABWC_FONT_H */
