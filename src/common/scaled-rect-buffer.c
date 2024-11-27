// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/graphic-helpers.h"
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/scaled-scene-buffer.h"
#include "common/scaled-rect-buffer.h"

static struct wl_list cached_buffers = WL_LIST_INIT(&cached_buffers);

static void
draw_rectangle_path(cairo_t *cairo, int width, int height, int border_width)
{
	double offset = border_width / 2.0;
	double right_x = width - offset;
	double bottom_y = height - offset;

	cairo_move_to(cairo, offset, offset);
	cairo_line_to(cairo, right_x, offset);
	cairo_line_to(cairo, right_x, bottom_y);
	cairo_line_to(cairo, offset, bottom_y);
	cairo_close_path(cairo);
}

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct scaled_rect_buffer *self = scaled_buffer->data;
	struct lab_data_buffer *buffer = buffer_create_cairo(
		self->width, self->height, scale);
	if (!buffer) {
		return NULL;
	}

	cairo_t *cairo = cairo_create(buffer->surface);

	/* Clear background */
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);

	/* Fill rectangle */
	draw_rectangle_path(cairo, self->width, self->height, 0);
	set_cairo_color(cairo, self->fill_color);
	cairo_fill(cairo);

	/* Draw borders */
	draw_rectangle_path(cairo, self->width, self->height,
		self->border_width);
	cairo_set_line_width(cairo, self->border_width);
	set_cairo_color(cairo, self->border_color);
	cairo_stroke(cairo);

	cairo_destroy(cairo);

	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_rect_buffer *self = scaled_buffer->data;
	scaled_buffer->data = NULL;
	free(self);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a, struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_rect_buffer *a = scaled_buffer_a->data;
	struct scaled_rect_buffer *b = scaled_buffer_b->data;

	return a->width == b->width
		&& a->height == b->height
		&& a->border_width == b->border_width
		&& !memcmp(a->fill_color, b->fill_color, sizeof(a->fill_color))
		&& !memcmp(a->border_color, b->border_color, sizeof(a->border_color));
}

static const struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

struct scaled_rect_buffer *scaled_rect_buffer_create(
	struct wlr_scene_tree *parent, int width, int height, int border_width,
	float fill_color[4], float border_color[4])
{
	/* TODO: support rounded corners for menus and OSDs */

	assert(parent);
	struct scaled_rect_buffer *self = znew(*self);
	struct scaled_scene_buffer *scaled_buffer = scaled_scene_buffer_create(
		parent, &impl, &cached_buffers, /* drop_buffer */ true);
	scaled_buffer->data = self;
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	self->width = MAX(width, 1);
	self->height = MAX(height, 1);
	self->border_width = border_width;
	memcpy(self->fill_color, fill_color, sizeof(self->fill_color));
	memcpy(self->border_color, border_color, sizeof(self->border_color));

	scaled_scene_buffer_invalidate_cache(scaled_buffer);

	return self;
}
