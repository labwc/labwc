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
#include "button/button-svg.h"
#include "button/common.h"
#include "common/file-helpers.h"
#include "labwc.h"
#include "theme.h"

void
button_svg_load(const char *button_name, struct lab_data_buffer **buffer,
		int size)
{
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}

	char filename[4096] = { 0 };
	button_filename(button_name, filename, sizeof(filename));

	GError *err = NULL;
	RsvgRectangle viewport = { .width = size, .height = size };
	RsvgHandle *svg = rsvg_handle_new_from_file(filename, &err);
	if (err) {
		wlr_log(WLR_DEBUG, "error reading svg %s-%s\n", filename, err->message);
		g_error_free(err);
		/*
		 * rsvg_handle_new_from_file() returns NULL if an error occurs,
		 * so there is no need to free svg here.
		 */
		return;
	}

	cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	cairo_t *cr = cairo_create(image);

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

	double w = cairo_image_surface_get_width(image);
	double h = cairo_image_surface_get_height(image);
	*buffer = buffer_create_cairo((int)w, (int)h, 1.0, /* free_on_destroy */ true);
	cairo_t *cairo = (*buffer)->cairo;
	cairo_set_source_surface(cairo, image, 0, 0);
	cairo_paint_with_alpha(cairo, 1.0);

error:
	cairo_destroy(cr);
	cairo_surface_destroy(image);
	g_object_unref(svg);
}
