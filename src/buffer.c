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

#include <assert.h>
#include <stdlib.h>
#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/box.h"
#include "common/mem.h"

static struct lab_data_buffer *data_buffer_from_buffer(
	struct wlr_buffer *buffer);

static void
data_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct lab_data_buffer *buffer = data_buffer_from_buffer(wlr_buffer);
	/* this also frees buffer->data if surface_owns_data == true */
	cairo_surface_destroy(buffer->surface);
	if (!buffer->surface_owns_data) {
		free(buffer->data);
	}
	wlr_buffer_finish(wlr_buffer);
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

static struct lab_data_buffer *
data_buffer_from_buffer(struct wlr_buffer *buffer)
{
	assert(buffer->impl == &data_buffer_impl);
	return (struct lab_data_buffer *)buffer;
}

struct lab_data_buffer *
buffer_adopt_cairo_surface(cairo_surface_t *surface)
{
	assert(cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE);
	assert(cairo_image_surface_get_format(surface) == CAIRO_FORMAT_ARGB32);

	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);

	struct lab_data_buffer *buffer = znew(*buffer);
	wlr_buffer_init(&buffer->base, &data_buffer_impl, width, height);

	buffer->surface = surface;
	buffer->data = cairo_image_surface_get_data(buffer->surface);
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = cairo_image_surface_get_stride(buffer->surface);
	buffer->logical_width = width;
	buffer->logical_height = height;
	buffer->surface_owns_data = true;

	return buffer;
}

struct lab_data_buffer *
buffer_create_cairo(uint32_t logical_width, uint32_t logical_height, float scale)
{
	/* Create an image surface with the scaled size */
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			lroundf(logical_width * scale),
			lroundf(logical_height * scale));

	/**
	 * Tell cairo about the device scale so we can keep drawing in unscaled
	 * coordinate space. Pango will automatically use the cairo scale attribute
	 * as well when creating text on this surface.
	 *
	 * For a more complete explanation see PR #389
	 */
	cairo_surface_set_device_scale(surface, scale, scale);

	/*
	 * Adopt the image surface into a buffer, set the correct
	 * logical size, and create a cairo context for drawing
	 */
	struct lab_data_buffer *buffer = buffer_adopt_cairo_surface(surface);
	buffer->logical_width = logical_width;
	buffer->logical_height = logical_height;

	return buffer;
}

struct lab_data_buffer *
buffer_create_from_data(void *pixel_data, uint32_t width, uint32_t height,
		uint32_t stride)
{
	struct lab_data_buffer *buffer = znew(*buffer);
	wlr_buffer_init(&buffer->base, &data_buffer_impl, width, height);
	buffer->logical_width = width;
	buffer->logical_height = height;
	buffer->data = pixel_data;
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;
	buffer->surface = cairo_image_surface_create_for_data(
		pixel_data, CAIRO_FORMAT_ARGB32, width, height, stride);
	buffer->surface_owns_data = false;
	return buffer;
}

struct lab_data_buffer *
buffer_create_from_wlr_buffer(struct wlr_buffer *wlr_buffer)
{
	void *data;
	uint32_t format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(wlr_buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		wlr_log(WLR_ERROR, "failed to access wlr_buffer");
		return NULL;
	}
	if (format != DRM_FORMAT_ARGB8888) {
		/* TODO: support other formats */
		wlr_buffer_end_data_ptr_access(wlr_buffer);
		wlr_log(WLR_ERROR, "cannot create buffer: format=%d", format);
		return NULL;
	}
	size_t buffer_size = stride * wlr_buffer->height;
	void *copied_data = xmalloc(buffer_size);
	memcpy(copied_data, data, buffer_size);
	wlr_buffer_end_data_ptr_access(wlr_buffer);

	return buffer_create_from_data(copied_data,
		wlr_buffer->width, wlr_buffer->height, stride);
}

struct lab_data_buffer *
buffer_resize(struct lab_data_buffer *src_buffer, int width, int height,
		double scale)
{
	assert(src_buffer);
	cairo_surface_t *surface = src_buffer->surface;

	int src_w = cairo_image_surface_get_width(surface);
	int src_h = cairo_image_surface_get_height(surface);

	struct lab_data_buffer *buffer =
		buffer_create_cairo(width, height, scale);
	cairo_t *cairo = cairo_create(buffer->surface);

	struct wlr_box container = {
		.width = width,
		.height = height,
	};

	struct wlr_box dst_box = box_fit_within(src_w, src_h, &container);
	double scene_scale = (double)dst_box.width / (double)src_w;
	cairo_translate(cairo, dst_box.x, dst_box.y);
	cairo_scale(cairo, scene_scale, scene_scale);
	cairo_set_source_surface(cairo, surface, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cairo), CAIRO_FILTER_GOOD);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);

	cairo_surface_flush(buffer->surface);
	cairo_destroy(cairo);

	return buffer;
}
