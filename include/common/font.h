/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_FONT_H
#define LABWC_FONT_H

struct lab_data_buffer;

enum font_slant {
	FONT_SLANT_NORMAL = 0,
	FONT_SLANT_ITALIC
};

enum font_weight {
	FONT_WEIGHT_NORMAL = 0,
	FONT_WEIGHT_BOLD
};

struct font {
	char *name;
	int size;
	enum font_slant slant;
	enum font_weight weight;
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
 * font_buffer_create - Create ARGB8888 lab_data_buffer using pango
 * @buffer: buffer pointer
 * @max_width: max allowable width; will be ellipsized if longer
 * @text: text to be generated as texture
 * @font: font description
 * @color: foreground color in rgba format
 * @bg_color: background color in rgba format
 * @arrow: arrow (utf8) character to show or NULL for none
 */
void font_buffer_create(struct lab_data_buffer **buffer, int max_width,
	const char *text, struct font *font, const float *color,
	const float *bg_color, const char *arrow, double scale);

/**
 * font_finish - free some font related resources
 * Note: use on exit
 */
void font_finish(void);

#endif /* LABWC_FONT_H */
