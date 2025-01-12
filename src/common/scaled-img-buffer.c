// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/mem.h"
#include "common/scaled-img-buffer.h"
#include "common/scaled-scene-buffer.h"
#include "img/img.h"
#include "node.h"

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct scaled_img_buffer *self = scaled_buffer->data;
	struct lab_data_buffer *buffer = lab_img_render(self->img,
		self->width, self->height, self->padding, scale);
	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_img_buffer *self = scaled_buffer->data;
	lab_img_destroy(self->img);
	free(self);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a,
	struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_img_buffer *a = scaled_buffer_a->data;
	struct scaled_img_buffer *b = scaled_buffer_b->data;

	return lab_img_equal(a->img, b->img)
		&& a->width == b->width
		&& a->height == b->height
		&& a->padding == b->padding;
}

static struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

struct scaled_img_buffer *
scaled_img_buffer_create(struct wlr_scene_tree *parent, struct lab_img *img,
	int width, int height, int padding)
{
	assert(img);
	struct scaled_scene_buffer *scaled_buffer = scaled_scene_buffer_create(
		parent, &impl, /* drop_buffer */ true);
	struct scaled_img_buffer *self = znew(*self);
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	self->img = lab_img_copy(img);
	self->width = width;
	self->height = height;
	self->padding = padding;

	scaled_buffer->data = self;

	scaled_scene_buffer_request_update(scaled_buffer, width, height);

	return self;
}

void
scaled_img_buffer_update(struct scaled_img_buffer *self, struct lab_img *img,
	int width, int height, int padding)
{
	assert(img);
	lab_img_destroy(self->img);
	self->img = lab_img_copy(img);
	self->width = width;
	self->height = height;
	self->padding = padding;
	scaled_scene_buffer_request_update(self->scaled_buffer, width, height);
}

struct scaled_img_buffer *
scaled_img_buffer_from_node(struct wlr_scene_node *node)
{
	struct scaled_scene_buffer *scaled_buffer =
		node_scaled_scene_buffer_from_node(node);
	assert(scaled_buffer->impl == &impl);
	return scaled_buffer->data;
}
