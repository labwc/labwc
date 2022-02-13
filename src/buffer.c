// SPDX-License-Identifier: GPL-2.0-only
/*
 * Based on wlroots/types/wlr_buffer.c
 *
 * Copyright (c) 2017, 2018 Drew DeVault
 * Copyright (c) 2018-2021 Simon Ser, Simon Zeni

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"
#include <assert.h>
#include <drm_fourcc.h>
#include "buffer.h"

static const struct wlr_buffer_impl data_buffer_impl;

static struct lab_data_buffer *
data_buffer_from_buffer(struct wlr_buffer *buffer)
{
	assert(buffer->impl == &data_buffer_impl);
	return (struct lab_data_buffer *)buffer;
}

static void
data_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct lab_data_buffer *buffer = data_buffer_from_buffer(wlr_buffer);
	free(buffer->data);
	free(buffer);
}

static bool
data_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride)
{
	struct lab_data_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	assert(buffer->data);
	*data = (void *)buffer->data;
	*format = buffer->format;
	*stride = buffer->stride;
	return true;
}

static void
data_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
	/* noop */
}

static const struct wlr_buffer_impl data_buffer_impl = {
	.destroy = data_buffer_destroy,
	.begin_data_ptr_access = data_buffer_begin_data_ptr_access,
	.end_data_ptr_access = data_buffer_end_data_ptr_access,
};

struct lab_data_buffer *
buffer_create(cairo_t *cairo)
{
	struct lab_data_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		return NULL;
	}
	cairo_surface_t *surf = cairo_get_target(cairo);
	int width = cairo_image_surface_get_width(surf);
	int height = cairo_image_surface_get_height(surf);
	wlr_buffer_init(&buffer->base, &data_buffer_impl, width, height);

	buffer->cairo = cairo;
	buffer->data = cairo_image_surface_get_data(surf);
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = cairo_image_surface_get_stride(surf);

	if (!buffer->data) {
		cairo_destroy(cairo);
		free(buffer);
	}
	return buffer;
}

void
buffer_destroy(struct lab_data_buffer *buffer)
{
	if (!buffer) {
		return;
	}
	cairo_destroy(buffer->cairo);
	wlr_buffer_drop(&buffer->base);
}
