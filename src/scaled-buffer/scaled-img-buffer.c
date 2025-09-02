// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "scaled-buffer/scaled-img-buffer.h"
#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/mem.h"
#include "img/img.h"
#include "node.h"
#include "scaled-buffer/scaled-buffer.h"

static struct lab_data_buffer *
_create_buffer(struct scaled_buffer *scaled_buffer, double scale)
{
	struct scaled_img_buffer *self = scaled_buffer->data;
	struct lab_data_buffer *buffer = lab_img_render(self->img,
		self->width, self->height, scale);
	return buffer;
}

static void
_destroy(struct scaled_buffer *scaled_buffer)
{
	struct scaled_img_buffer *self = scaled_buffer->data;
	lab_img_destroy(self->img);
	free(self);
}

static bool
_equal(struct scaled_buffer *scaled_buffer_a,
	struct scaled_buffer *scaled_buffer_b)
{
	struct scaled_img_buffer *a = scaled_buffer_a->data;
	struct scaled_img_buffer *b = scaled_buffer_b->data;

	return lab_img_equal(a->img, b->img)
		&& a->width == b->width
		&& a->height == b->height;
}

static struct scaled_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

struct scaled_img_buffer *
scaled_img_buffer_create(struct wlr_scene_tree *parent, struct lab_img *img,
	int width, int height)
{
	assert(parent);
	assert(img);
	assert(width >= 0 && height >= 0);

	struct scaled_buffer *scaled_buffer = scaled_buffer_create(
		parent, &impl, /* drop_buffer */ true);
	struct scaled_img_buffer *self = znew(*self);
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	self->img = lab_img_copy(img);
	self->width = width;
	self->height = height;

	scaled_buffer->data = self;

	scaled_buffer_request_update(scaled_buffer, width, height);

	return self;
}

struct scaled_img_buffer *
scaled_img_buffer_from_node(struct wlr_scene_node *node)
{
	struct scaled_buffer *scaled_buffer =
		node_scaled_buffer_from_node(node);
	assert(scaled_buffer->impl == &impl);
	return scaled_buffer->data;
}
