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
#ifndef __LABWC_BUFFER_H
#define __LABWC_BUFFER_H

#include <cairo.h>
#include "labwc.h"

struct lab_data_buffer {
	struct wlr_buffer base;

	cairo_t *cairo;
	void *data;
	uint32_t format;
	size_t stride;
	bool free_on_destroy;
};

/* Create a buffer which creates a new cairo CAIRO_FORMAT_ARGB32 surface */
struct lab_data_buffer *buffer_create_cairo(uint32_t width, uint32_t height,
	float scale, bool free_on_destroy);

/* Create a buffer which wraps a given DRM_FORMAT_ARGB8888 pointer */
struct lab_data_buffer *buffer_create_wrap(void *pixel_data, uint32_t width,
	uint32_t height, uint32_t stride, bool free_on_destroy);

#endif /* __LABWC_BUFFER_H */
