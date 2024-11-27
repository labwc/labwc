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
#include "buffer.h"
#include "common/box.h"
#include "common/mem.h"

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
	/* this also frees buffer->data if surface_owns_data == true */
	cairo_surface_destroy(buffer->surface);
	if (!buffer->surface_owns_data) {
		free(buffer->data);
	}
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
buffer_convert_cairo_surface_for_icon(cairo_surface_t *surface,
		uint32_t icon_size, float scale)
{
	assert(cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE);

	/*
	 * Compute logical size for display and decide whether we can
	 * use the image data directly (fast path). Requirements are:
	 *
	 *  - The pixel format must be ARGB32.
	 *  - The image must not be so large as to need downsampling by
	 *    more than 2x when displayed at the target scale. wlr_scene
	 *    uses linear interpolation without pixel averaging, which
	 *    starts to skip samples if downsampling more than 2x,
	 *    resulting in a grainy look.
	 */
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	struct wlr_box logical =
		box_fit_within(width, height, icon_size, icon_size);
	struct lab_data_buffer *buffer;

	if (cairo_image_surface_get_format(surface) == CAIRO_FORMAT_ARGB32
			&& width <= 2 * logical.width * scale
			&& height <= 2 * logical.height * scale) {
		buffer = buffer_adopt_cairo_surface(surface);
		/* set logical size for display */
		buffer->logical_width = logical.width;
		buffer->logical_height = logical.height;
	} else {
		/* convert to ARGB32 and scale for display (slow path) */
		buffer = buffer_create_cairo(logical.width,
			logical.height, scale);

		cairo_t *cairo = cairo_create(buffer->surface);
		cairo_scale(cairo, (double)logical.width / width,
			(double)logical.height / height);
		cairo_set_source_surface(cairo, surface, 0, 0);
		cairo_pattern_set_filter(cairo_get_source(cairo),
			CAIRO_FILTER_GOOD);
		cairo_paint(cairo);

		/* ensure pixel data is updated */
		cairo_surface_flush(buffer->surface);
		/* destroy original cairo surface & context */
		cairo_surface_destroy(surface);
		cairo_destroy(cairo);
	}

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
