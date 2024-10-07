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

	cairo_surface_t *surface; /* optional */
	cairo_t *cairo;           /* optional */
	void *data; /* owned by surface if surface != NULL */
	uint32_t format;
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
 * device scale. No cairo context is created.
 */
struct lab_data_buffer *buffer_adopt_cairo_surface(cairo_surface_t *surface);

/*
 * Create a buffer which holds a new CAIRO_FORMAT_ARGB32 image surface.
 * Additionally create a cairo context for drawing to the surface.
 */
struct lab_data_buffer *buffer_create_cairo(uint32_t logical_width,
	uint32_t logical_height, float scale);

/*
 * Create a buffer from an image surface, for display as an icon.
 *
 * The surface is either adopted by the buffer (which takes ownership),
 * or copied and then destroyed.
 *
 * This function allows non-ARGB32 source images and converts to
 * CAIRO_FORMAT_ARGB32 if needed.
 */
struct lab_data_buffer *buffer_convert_cairo_surface_for_icon(
	cairo_surface_t *surface, uint32_t icon_size, float scale);

/*
 * Create a buffer which holds (and takes ownership of) raw pixel data
 * in pre-multiplied ARGB32 format.
 *
 * The logical size is set to the width and height of the pixel data.
 * No cairo surface or context is created.
 */
struct lab_data_buffer *buffer_create_from_data(void *pixel_data, uint32_t width,
	uint32_t height, uint32_t stride);

#endif /* LABWC_BUFFER_H */
