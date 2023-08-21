// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Johan Malm 2023
 */
#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "button/button-png.h"
#include "button/common.h"
#include "common/file-helpers.h"
#include "labwc.h"
#include "theme.h"

/*
 * cairo_image_surface_create_from_png() does not gracefully handle non-png
 * files, so we verify the header before trying to read the rest of the file.
 */
#define PNG_BYTES_TO_CHECK (4)
static bool
ispng(const char *filename)
{
	unsigned char header[PNG_BYTES_TO_CHECK];
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		return false;
	}
	if (fread(header, 1, PNG_BYTES_TO_CHECK, fp) != PNG_BYTES_TO_CHECK) {
		fclose(fp);
		return false;
	}
	if (png_sig_cmp(header, (png_size_t)0, PNG_BYTES_TO_CHECK)) {
		wlr_log(WLR_ERROR, "file '%s' is not a recognised png file", filename);
		fclose(fp);
		return false;
	}
	fclose(fp);
	return true;
}

#undef PNG_BYTES_TO_CHECK

void
png_load(const char *button_name, struct lab_data_buffer **buffer)
{
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}

	char path[4096] = { 0 };
	button_filename(button_name, path, sizeof(path));
	if (!file_exists(path) || !ispng(path)) {
		return;
	}

	cairo_surface_t *image = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(image)) {
		wlr_log(WLR_ERROR, "error reading png button '%s'", path);
		cairo_surface_destroy(image);
		return;
	}
	cairo_surface_flush(image);

	double w = cairo_image_surface_get_width(image);
	double h = cairo_image_surface_get_height(image);
	*buffer = buffer_create_cairo((int)w, (int)h, 1.0, true);
	cairo_t *cairo = (*buffer)->cairo;
	cairo_set_source_surface(cairo, image, 0, 0);
	cairo_paint_with_alpha(cairo, 1.0);
}
