// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/font.h"
#include "common/mem.h"
#include "common/scaled-scene-buffer.h"
#include "common/scaled-font-buffer.h"

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct lab_data_buffer *buffer;
	struct scaled_font_buffer *self = scaled_buffer->data;

	/* Buffer gets free'd automatically along the backing wlr_buffer */
	font_buffer_create(&buffer, self->max_width, self->text,
		&self->font, self->color, self->bg_color, self->arrow, scale);

	self->width = buffer ? buffer->unscaled_width : 0;
	self->height = buffer ? buffer->unscaled_height : 0;
	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_font_buffer *self = scaled_buffer->data;
	scaled_buffer->data = NULL;

	zfree(self->text);
	zfree(self->font.name);
	zfree(self->arrow);
	free(self);
}

static const struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy
};

/* Public API */
struct scaled_font_buffer *
scaled_font_buffer_create(struct wlr_scene_tree *parent)
{
	assert(parent);
	struct scaled_font_buffer *self = znew(*self);
	struct scaled_scene_buffer *scaled_buffer =
		scaled_scene_buffer_create(parent, &impl, /* drop_buffer */ true);
	if (!scaled_buffer) {
		free(self);
		return NULL;
	}

	scaled_buffer->data = self;
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	return self;
}

void
scaled_font_buffer_update(struct scaled_font_buffer *self, const char *text,
		int max_width, struct font *font, const float *color,
		const float *bg_color, const char *arrow)
{
	assert(self);
	assert(text);
	assert(font);
	assert(color);

	/* Clean up old internal state */
	zfree(self->text);
	zfree(self->font.name);
	zfree(self->arrow);

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
	self->arrow = arrow ? xstrdup(arrow) : NULL;

	/* Invalidate cache and force a new render */
	scaled_scene_buffer_invalidate_cache(self->scaled_buffer);
}

void
scaled_font_buffer_set_max_width(struct scaled_font_buffer *self, int max_width)
{
	self->max_width = max_width;
	scaled_scene_buffer_invalidate_cache(self->scaled_buffer);
}
