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
	free(buffer->saved_data);
	free(buffer);
}

static bool
data_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride)
{
	struct lab_data_buffer *buffer = data_buffer_from_buffer(wlr_buffer);
	if (buffer->data == NULL) {
		return false;
	}
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
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
buffer_create(uint32_t format, size_t stride, uint32_t width, uint32_t height,
		const void *data)
{
	struct lab_data_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return NULL;
	}
	wlr_buffer_init(&buffer->base, &data_buffer_impl, width, height);
	buffer->data = data;
	buffer->format = format;
	buffer->stride = stride;
	return buffer;
}


bool
buffer_drop(struct lab_data_buffer *buffer)
{
	bool ok = true;

	if (buffer->base.n_locks > 0) {
		size_t size = buffer->stride * buffer->base.height;
		buffer->saved_data = malloc(size);
		if (!buffer->saved_data) {
			wlr_log_errno(WLR_ERROR, "allocation failed");
			ok = false;
			buffer->data = NULL;
		} else {
			memcpy(buffer->saved_data, buffer->data, size);
			buffer->data = buffer->saved_data;
		}
	}
	wlr_buffer_drop(&buffer->base);
	return ok;
}
