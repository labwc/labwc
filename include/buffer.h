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

#include "labwc.h"

/**
 * A read-only buffer that holds a data pointer.
 *
 * This is suitable for passing raw pixel data to a function that accepts a
 * wlr_buffer.
 */
struct lab_data_buffer {
	struct wlr_buffer base;

	const void *data;
	uint32_t format;
	size_t stride;

	void *saved_data;
};

/**
 * Wraps a read-only data pointer into a wlr_buffer. The data pointer may be
 * accessed until readonly_data_buffer_drop() is called.
 */
struct lab_data_buffer *buffer_create(uint32_t format, size_t stride,
	uint32_t width, uint32_t height, const void *data);

/**
 * Drops ownership of the buffer (see wlr_buffer_drop() for more details) and
 * perform a copy of the data pointer if a consumer still has the buffer locked.
 */
bool buffer_drop(struct lab_data_buffer *buffer);

#endif /* __LABWC_BUFFER_H */
