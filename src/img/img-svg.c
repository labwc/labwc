// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Johan Malm 2023
 */
#define _POSIX_C_SOURCE 200809L
#include "img/img-svg.h"
#include <cairo.h>
#include <librsvg/rsvg.h>
#include <stdio.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/string-helpers.h"

RsvgHandle *
img_svg_load(const char *filename)
{
	if (string_null_or_empty(filename)) {
		return NULL;
	}

	GError *err = NULL;
	RsvgHandle *svg = rsvg_handle_new_from_file(filename, &err);
	if (err) {
		wlr_log(WLR_DEBUG, "error reading svg %s-%s", filename, err->message);
		g_error_free(err);
		/*
		 * rsvg_handle_new_from_file() returns NULL if an error occurs,
		 * so there is no need to free svg here.
		 */
		return NULL;
	}
	return svg;
}

struct lab_data_buffer *
img_svg_render(RsvgHandle *svg, int w, int h, double scale)
{
	struct lab_data_buffer *buffer = buffer_create_cairo(w, h, scale);
	cairo_surface_t *image = buffer->surface;
	cairo_t *cr = cairo_create(image);
	GError *err = NULL;

	RsvgRectangle viewport = {
		.width = w,
		.height = h,
	};
	rsvg_handle_render_document(svg, cr, &viewport, &err);
	if (err) {
		wlr_log(WLR_ERROR, "error rendering svg: %s", err->message);
		g_error_free(err);
		goto error;
	}
	if (cairo_surface_status(image)) {
		wlr_log(WLR_ERROR, "error reading svg file");
		goto error;
	}
	cairo_surface_flush(buffer->surface);
	cairo_destroy(cr);

	return buffer;

error:
	wlr_buffer_drop(&buffer->base);
	cairo_destroy(cr);
	return NULL;
}
