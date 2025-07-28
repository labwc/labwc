// SPDX-License-Identifier: GPL-2.0-only

#include "img/img.h"
#include <assert.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "config.h"
#include "common/box.h"
#include "common/graphic-helpers.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "img/img-png.h"
#if HAVE_RSVG
#include "img/img-svg.h"
#endif
#include "img/img-xbm.h"
#include "img/img-xpm.h"
#include "labwc.h"
#include "theme.h"

struct lab_img_data {
	enum lab_img_type type;
	/* lab_img_data is refcounted to be shared by multiple lab_imgs */
	int refcount;

	/* Handler for the loaded image file */
	struct lab_data_buffer *buffer; /* for PNG/XBM/XPM image */
#if HAVE_RSVG
	RsvgHandle *svg; /* for SVG image */
#endif
};

static struct lab_img *
create_img(struct lab_img_data *img_data)
{
	struct lab_img *img = znew(*img);
	img->data = img_data;
	img_data->refcount++;
	wl_array_init(&img->modifiers);
	return img;
}

struct lab_img *
lab_img_load(enum lab_img_type type, const char *path, float *xbm_color)
{
	if (string_null_or_empty(path)) {
		return NULL;
	}

	struct lab_img_data *img_data = znew(*img_data);
	img_data->type = type;

	switch (type) {
	case LAB_IMG_PNG:
		img_data->buffer = img_png_load(path);
		break;
	case LAB_IMG_XBM:
		assert(xbm_color);
		img_data->buffer = img_xbm_load(path, xbm_color);
		break;
	case LAB_IMG_XPM:
		img_data->buffer = img_xpm_load(path);
		break;
	case LAB_IMG_SVG:
#if HAVE_RSVG
		img_data->svg = img_svg_load(path);
#endif
		break;
	}

	bool img_is_loaded = (bool)img_data->buffer;
#if HAVE_RSVG
	img_is_loaded |= (bool)img_data->svg;
#endif

	if (img_is_loaded) {
		return create_img(img_data);
	} else {
		free(img_data);
		return NULL;
	}
}

struct lab_img *
lab_img_load_from_bitmap(const char *bitmap, float *rgba)
{
	struct lab_data_buffer *buffer = img_xbm_load_from_bitmap(bitmap, rgba);
	if (!buffer) {
		return NULL;
	}

	struct lab_img_data *img_data = znew(*img_data);
	img_data->type = LAB_IMG_XBM;
	img_data->buffer = buffer;

	return create_img(img_data);
}

struct lab_img *
lab_img_copy(struct lab_img *img)
{
	struct lab_img *new_img = create_img(img->data);
	wl_array_copy(&new_img->modifiers, &img->modifiers);
	return new_img;
}

void
lab_img_add_modifier(struct lab_img *img,  lab_img_modifier_func_t modifier)
{
	lab_img_modifier_func_t *mod = wl_array_add(&img->modifiers, sizeof(*mod));
	*mod = modifier;
}

struct lab_data_buffer *
lab_img_render(struct lab_img *img, int width, int height, double scale)
{
	struct lab_data_buffer *buffer = NULL;

	/* Render the image into the buffer for the given size */
	switch (img->data->type) {
	case LAB_IMG_PNG:
	case LAB_IMG_XBM:
	case LAB_IMG_XPM:
		buffer = buffer_resize(img->data->buffer, width, height, scale);
		break;
#if HAVE_RSVG
	case LAB_IMG_SVG:
		buffer = img_svg_render(img->data->svg, width, height, scale);
		break;
#endif
	default:
		break;
	}

	if (!buffer) {
		return NULL;
	}

	/* Apply modifiers to the buffer (e.g. draw hover overlay) */
	cairo_t *cairo = cairo_create(buffer->surface);
	lab_img_modifier_func_t *modifier;
	wl_array_for_each(modifier, &img->modifiers) {
		cairo_save(cairo);
		(*modifier)(cairo, width, height);
		cairo_restore(cairo);
	}

	cairo_surface_flush(buffer->surface);
	cairo_destroy(cairo);

	return buffer;
}

void
lab_img_destroy(struct lab_img *img)
{
	if (!img) {
		return;
	}

	img->data->refcount--;
	if (img->data->refcount == 0) {
		if (img->data->buffer) {
			wlr_buffer_drop(&img->data->buffer->base);
		}
#if HAVE_RSVG
		if (img->data->svg) {
			g_object_unref(img->data->svg);
		}
#endif
		free(img->data);
	}

	wl_array_release(&img->modifiers);
	free(img);
}

bool
lab_img_equal(struct lab_img *img_a, struct lab_img *img_b)
{
	if (img_a == img_b) {
		return true;
	}
	if (!img_a || !img_b || img_a->data != img_b->data
			|| img_a->modifiers.size != img_b->modifiers.size) {
		return false;
	}
	return img_a->modifiers.size == 0
		|| !memcmp(img_a->modifiers.data, img_b->modifiers.data,
			img_a->modifiers.size);
}
