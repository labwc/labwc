// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Johan Malm 2023
 */
#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <librsvg/rsvg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "img/img-svg.h"
#include "common/string-helpers.h"
#include "labwc.h"

void
img_svg_load(const char *filename, struct lab_data_buffer **buffer,
		int size)
{
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}
	if (string_null_or_empty(filename)) {
		return;
	}

	GError *err = NULL;
	RsvgRectangle viewport = { .width = size, .height = size };
	RsvgHandle *svg = rsvg_handle_new_from_file(filename, &err);
	if (err) {
		wlr_log(WLR_DEBUG, "error reading svg %s-%s", filename, err->message);
		g_error_free(err);
		/*
		 * rsvg_handle_new_from_file() returns NULL if an error occurs,
		 * so there is no need to free svg here.
		 */
		return;
	}

	*buffer = buffer_create_cairo(size, size, 1.0);
	cairo_surface_t *image = (*buffer)->surface;
	cairo_t *cr = (*buffer)->cairo;

	rsvg_handle_render_document(svg, cr, &viewport, &err);
	if (err) {
		wlr_log(WLR_ERROR, "error rendering svg %s-%s\n", filename, err->message);
		g_error_free(err);
		goto error;
	}

	if (cairo_surface_status(image)) {
		wlr_log(WLR_ERROR, "error reading svg button '%s'", filename);
		goto error;
	}
	cairo_surface_flush(image);

	g_object_unref(svg);
	return;

error:
	wlr_buffer_drop(&(*buffer)->base);
	*buffer = NULL;
	g_object_unref(svg);
}
