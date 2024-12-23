/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Based on wlroots/include/types/wlr_buffer.c
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
#ifndef LABWC_BUFFER_H
#define LABWC_BUFFER_H

#include <cairo.h>
#include <wlr/types/wlr_buffer.h>

struct lab_data_buffer {
	struct wlr_buffer base;

	bool surface_owns_data;
	cairo_surface_t *surface;
	void *data;
	uint32_t format; /* currently always DRM_FORMAT_ARGB8888 */
	size_t stride;
	/*
	 * The logical size of the surface in layout pixels.
	 * The raw pixel data may be larger or smaller.
	 */
	uint32_t logical_width;
	uint32_t logical_height;
};

/*
 * Create a buffer which holds (and takes ownership of) an existing
 * CAIRO_FORMAT_ARGB32 image surface.
 *
 * The logical size is set to the surface size in pixels, ignoring
 * device scale.
 */
struct lab_data_buffer *buffer_adopt_cairo_surface(cairo_surface_t *surface);

/*
 * Create a buffer which holds a new CAIRO_FORMAT_ARGB32 image surface.
 * Additionally create a cairo context for drawing to the surface.
 */
struct lab_data_buffer *buffer_create_cairo(uint32_t logical_width,
	uint32_t logical_height, float scale);

/*
 * Create a buffer which holds (and takes ownership of) raw pixel data
 * in pre-multiplied ARGB32 format.
 *
 * The logical size is set to the width and height of the pixel data.
 */
struct lab_data_buffer *buffer_create_from_data(void *pixel_data, uint32_t width,
	uint32_t height, uint32_t stride);

#endif /* LABWC_BUFFER_H */
