/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_FONT_H
#define __LABWC_FONT_H

struct lab_data_buffer;

struct font {
	char *name;
	int size;
};

/**
 * font_height - get font vertical extents
 * @font: description of font including family name and size
 */
int font_height(struct font *font);

/**
 * font_buffer_create - Create ARGB8888 lab_data_buffer using pango
 * @buffer: buffer pointer
 * @max_width: max allowable width; will be ellipsized if longer
 * @text: text to be generated as texture
 * @font: font description
 * @color: foreground color in rgba format
 */
void font_buffer_create(struct lab_data_buffer **buffer, int max_width,
	const char *text, struct font *font, float *color, double scale);

/**
 * font_buffer_update - Wrapper around font_buffer_create
 * Only difference is that if given buffer pointer is != NULL
 * wlr_buffer_drop() will be called on the buffer.
 */
void font_buffer_update(struct lab_data_buffer **buffer, int max_width,
	const char *text, struct font *font, float *color, double scale);

/**
 * font_finish - free some font related resources
 * Note: use on exit
 */
void font_finish(void);

#endif /* __LABWC_FONT_H */
