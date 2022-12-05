/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LAB_COMMON_SCALED_FONT_BUFFER_H
#define __LAB_COMMON_SCALED_FONT_BUFFER_H

#include "common/font.h"

struct wlr_scene_tree;
struct wlr_scene_buffer;
struct scaled_scene_buffere;

struct scaled_font_buffer {
	struct wlr_scene_buffer *scene_buffer;
	int width;   /* unscaled, read only */
	int height;  /* unscaled, read only */

	/* Private */
	char *text;
	int max_width;
	float color[4];
	char *arrow;
	struct font font;
	struct scaled_scene_buffer *scaled_buffer;
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
 */
void scaled_font_buffer_update(struct scaled_font_buffer *self, const char *text,
	int max_width, struct font *font, float *color, const char *arrow);

/**
 * Update the max width of an existing auto scaling font buffer
 * and force a new render.
 *
 * No steps are taken to detect if its actually required to render a new buffer.
 */
void scaled_font_buffer_set_max_width(struct scaled_font_buffer *self, int max_width);

#endif /* __LAB_COMMON_SCALED_FONT_BUFFER_H */
