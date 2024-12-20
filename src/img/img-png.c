// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Johan Malm 2023
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/string-helpers.h"
#include "img/img-png.h"

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

struct lab_data_buffer *
img_png_load(const char *filename)
{
	if (string_null_or_empty(filename)) {
		return NULL;
	}
	if (!ispng(filename)) {
		return NULL;
	}

	cairo_surface_t *image = cairo_image_surface_create_from_png(filename);
	if (cairo_surface_status(image)) {
		wlr_log(WLR_ERROR, "error reading png file '%s'", filename);
		cairo_surface_destroy(image);
		return NULL;
	}

	return buffer_adopt_cairo_surface(image);
}
