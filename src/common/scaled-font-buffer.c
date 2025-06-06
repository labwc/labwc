// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/mem.h"
#include "common/scaled-scene-buffer.h"
#include "common/scaled-font-buffer.h"
#include "common/string-helpers.h"

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct lab_data_buffer *buffer = NULL;
	struct scaled_font_buffer *self = scaled_buffer->data;
	cairo_pattern_t *bg_pattern = self->bg_pattern;
	cairo_pattern_t *solid_bg_pattern = NULL;

	if (!bg_pattern) {
		solid_bg_pattern = color_to_pattern(self->bg_color);
		bg_pattern = solid_bg_pattern;
	}

	/* Buffer gets free'd automatically along the backing wlr_buffer */
	font_buffer_create(&buffer, self->max_width, self->height, self->text,
		&self->font, self->color, bg_pattern, scale);

	if (!buffer) {
		wlr_log(WLR_ERROR, "font_buffer_create() failed");
	}

	zfree_pattern(solid_bg_pattern);
	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_font_buffer *self = scaled_buffer->data;
	scaled_buffer->data = NULL;

	zfree(self->text);
	zfree(self->font.name);
	zfree_pattern(self->bg_pattern);
	free(self);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a,
	struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_font_buffer *a = scaled_buffer_a->data;
	struct scaled_font_buffer *b = scaled_buffer_b->data;

	return str_equal(a->text, b->text)
		&& a->max_width == b->max_width
		&& str_equal(a->font.name, b->font.name)
		&& a->font.size == b->font.size
		&& a->font.slant == b->font.slant
		&& a->font.weight == b->font.weight
		&& !memcmp(a->color, b->color, sizeof(a->color))
		&& !memcmp(a->bg_color, b->bg_color, sizeof(a->bg_color))
		&& a->fixed_height == b->fixed_height
		&& a->bg_pattern == b->bg_pattern;
}

static const struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

/* Public API */
struct scaled_font_buffer *
scaled_font_buffer_create(struct wlr_scene_tree *parent)
{
	assert(parent);
	struct scaled_font_buffer *self = znew(*self);
	struct scaled_scene_buffer *scaled_buffer = scaled_scene_buffer_create(
		parent, &impl, /* drop_buffer */ true);
	if (!scaled_buffer) {
		free(self);
		return NULL;
	}

	scaled_buffer->data = self;
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	return self;
}

struct scaled_font_buffer *
scaled_font_buffer_create_for_titlebar(struct wlr_scene_tree *parent,
		int fixed_height, cairo_pattern_t *bg_pattern)
{
	struct scaled_font_buffer *self = scaled_font_buffer_create(parent);
	if (self) {
		self->fixed_height = fixed_height;
		self->bg_pattern = cairo_pattern_reference(bg_pattern);
	}
	return self;
}

void
scaled_font_buffer_update(struct scaled_font_buffer *self, const char *text,
		int max_width, struct font *font, const float *color,
		const float *bg_color)
{
	assert(self);
	assert(text);
	assert(font);
	assert(color);

	/* Clean up old internal state */
	zfree(self->text);
	zfree(self->font.name);

	/* Update internal state */
	self->text = xstrdup(text);
	self->max_width = max_width;
	if (font->name) {
		self->font.name = xstrdup(font->name);
	}
	self->font.size = font->size;
	self->font.slant = font->slant;
	self->font.weight = font->weight;
	memcpy(self->color, color, sizeof(self->color));
	memcpy(self->bg_color, bg_color, sizeof(self->bg_color));

	/* Calculate the size of font buffer and request re-rendering */
	int computed_height;
	font_get_buffer_size(self->max_width, self->text, &self->font,
		&self->width, &computed_height);
	self->height = (self->fixed_height > 0) ?
		self->fixed_height : computed_height;
	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

void
scaled_font_buffer_set_max_width(struct scaled_font_buffer *self, int max_width)
{
	self->max_width = max_width;

	int computed_height;
	font_get_buffer_size(self->max_width, self->text, &self->font,
		&self->width, &computed_height);
	self->height = (self->fixed_height > 0) ?
		self->fixed_height : computed_height;
	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}
