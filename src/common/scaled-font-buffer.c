// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/font.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scaled-scene-buffer.h"
#include "common/scaled-font-buffer.h"

/* This holds all the scaled-font-buffers and used for sharing backing buffers */
static struct wl_list cached_buffers = WL_LIST_INIT(&cached_buffers);

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct lab_data_buffer *buffer = NULL;
	struct scaled_font_buffer *self = scaled_buffer->data;

	/* Buffer gets free'd automatically along the backing wlr_buffer */
	font_buffer_create(&buffer, self->max_width, self->text,
		&self->font, self->color, self->bg_color, scale);

	if (!buffer) {
		wlr_log(WLR_ERROR, "font_buffer_create() failed");
	}

	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_font_buffer *self = scaled_buffer->data;
	scaled_buffer->data = NULL;

	zfree(self->text);
	zfree(self->font.name);
	free(self);
}

static bool str_equal(const char *a, const char *b)
{
	return a == b || (a && b && !strcmp(a, b));
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
		&& !memcmp(a->bg_color, b->bg_color, sizeof(a->bg_color));
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
		parent, &impl, &cached_buffers, /* drop_buffer */ true);
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
	font_get_buffer_size(self->max_width, self->text, &self->font,
		&self->width, &self->height);
	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

void
scaled_font_buffer_set_max_width(struct scaled_font_buffer *self, int max_width)
{
	self->max_width = max_width;

	font_get_buffer_size(self->max_width, self->text, &self->font,
		&self->width, &self->height);
	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}
