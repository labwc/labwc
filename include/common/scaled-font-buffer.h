/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCALED_FONT_BUFFER_H
#define LABWC_SCALED_FONT_BUFFER_H

#include "common/font.h"

struct wlr_scene_tree;
struct wlr_scene_buffer;
struct scaled_scene_buffer;

struct scaled_font_buffer {
	struct wlr_scene_buffer *scene_buffer;
	int width;   /* unscaled, read only */
	int height;  /* unscaled, read only */

	/* Private */
	char *text;
	int max_width;
	float color[4];
	float bg_color[4];
	struct font font;
	struct scaled_scene_buffer *scaled_buffer;

	/*
	 * The following fields are used only for the titlebar, where
	 * the font buffer can be rendered with a pattern background to
	 * support gradients. In this case, the font buffer is also
	 * padded to a fixed height (with the text centered vertically)
	 * in order to align the pattern with the rest of the titlebar.
	 */
	int fixed_height;
	cairo_pattern_t *bg_pattern; /* overrides bg_color if set */
};

/**
 * Create an auto scaling font buffer, providing a wlr_scene_buffer node for
 * display. It gets destroyed automatically when the backing scaled_scene_buffer
 * is being destroyed which in turn happens automatically when the backing
 * wlr_scene_buffer (or one of its parents) is being destroyed.
 *
 * To actually show some text, scaled_font_buffer_update() has to be called.
 *
 */
struct scaled_font_buffer *scaled_font_buffer_create(struct wlr_scene_tree *parent);

/**
 * Create an auto scaling font buffer for titlebar text.
 * The font buffer takes a new reference to bg_pattern.
 *
 * @param fixed_height Fixed height for the buffer (logical pixels)
 * @param bg_pattern Background pattern (solid color or gradient)
 */
struct scaled_font_buffer *
scaled_font_buffer_create_for_titlebar(struct wlr_scene_tree *parent,
	int fixed_height, cairo_pattern_t *bg_pattern);

/**
 * Update an existing auto scaling font buffer.
 *
 * No steps are taken to detect if its actually required to render a new buffer.
 * This should be done by the caller to prevent useless recreation of the same
 * buffer in case nothing actually changed.
 *
 * Some basic checks could be something like
 * - truncated = buffer->width == max_width
 * - text_changed = strcmp(old_text, new_text)
 * - font and color the same
 *
 * bg_color is ignored for font buffers created with
 * scaled_font_buffer_create_for_titlebar().
 */
void scaled_font_buffer_update(struct scaled_font_buffer *self, const char *text,
	int max_width, struct font *font, const float *color,
	const float *bg_color);

/**
 * Update the max width of an existing auto scaling font buffer
 * and force a new render.
 *
 * No steps are taken to detect if its actually required to render a new buffer.
 */
void scaled_font_buffer_set_max_width(struct scaled_font_buffer *self, int max_width);

#endif /* LABWC_SCALED_FONT_BUFFER_H */
