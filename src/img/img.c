// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "config.h"
#include "common/box.h"
#include "common/graphic-helpers.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "img/img.h"
#include "img/img-png.h"
#if HAVE_RSVG
#include "img/img-svg.h"
#endif
#include "img/img-xbm.h"
#include "img/img-xpm.h"
#include "labwc.h"
#include "theme.h"

struct lab_img_cache {
	enum lab_img_type type;
	/* lab_img_cache is refcounted to be shared by multiple lab_imgs */
	int refcount;

	/* Handler for the loaded image file */
	struct lab_data_buffer *buffer; /* for PNG/XBM/XPM image */
#if HAVE_RSVG
	RsvgHandle *svg; /* for SVG image */
#endif
};

static struct lab_img *
create_img(struct lab_img_cache *cache)
{
	struct lab_img *img = znew(*img);
	img->cache = cache;
	cache->refcount++;
	wl_array_init(&img->modifiers);
	return img;
}

struct lab_img *
lab_img_load(enum lab_img_type type, const char *path, float *xbm_color)
{
	if (string_null_or_empty(path)) {
		return NULL;
	}

	struct lab_img_cache *cache = znew(*cache);
	cache->type = type;

	switch (type) {
	case LAB_IMG_PNG:
		cache->buffer = img_png_load(path);
		break;
	case LAB_IMG_XBM:
		assert(xbm_color);
		cache->buffer = img_xbm_load(path, xbm_color);
		break;
	case LAB_IMG_XPM:
		cache->buffer = img_xpm_load(path);
		break;
	case LAB_IMG_SVG:
#if HAVE_RSVG
		cache->svg = img_svg_load(path);
#endif
		break;
	}

	bool img_is_loaded = (bool)cache->buffer;
#if HAVE_RSVG
	img_is_loaded |= (bool)cache->svg;
#endif

	if (img_is_loaded) {
		return create_img(cache);
	} else {
		free(cache);
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

	struct lab_img_cache *cache = znew(*cache);
	cache->type = LAB_IMG_XBM;
	cache->buffer = buffer;

	return create_img(cache);
}

struct lab_img *
lab_img_copy(struct lab_img *img)
{
	struct lab_img *new_img = create_img(img->cache);
	wl_array_copy(&new_img->modifiers, &img->modifiers);
	return new_img;
}

void
lab_img_add_modifier(struct lab_img *img,  lab_img_modifier_func_t modifier,
	struct theme *theme)
{
	img->theme = theme;
	lab_img_modifier_func_t *mod = wl_array_add(&img->modifiers, sizeof(*mod));
	*mod = modifier;
}

/*
 * Takes a source surface from PNG/XBM/XPM file and output a buffer for the
 * given size. The source surface is placed at the center of the output buffer
 * and shrunk if it overflows from the output buffer.
 */
static struct lab_data_buffer *
render_cairo_surface(cairo_surface_t *surface, int width, int height,
	int padding, double scale)
{
	assert(surface);
	int src_w = cairo_image_surface_get_width(surface);
	int src_h = cairo_image_surface_get_height(surface);

	struct lab_data_buffer *buffer =
		buffer_create_cairo(width, height, scale);
	cairo_t *cairo = cairo_create(buffer->surface);

	struct wlr_box container = {
		.x = padding,
		.y = padding,
		.width = width - 2 * padding,
		.height = height - 2 * padding,
	};

	struct wlr_box dst_box = box_fit_within(src_w, src_h, &container);
	double scene_scale = (double)dst_box.width / (double)src_w;
	cairo_translate(cairo, dst_box.x, dst_box.y);
	cairo_scale(cairo, scene_scale, scene_scale);
	cairo_set_source_surface(cairo, surface, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cairo), CAIRO_FILTER_GOOD);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);

	cairo_destroy(cairo);

	return buffer;
}

struct lab_data_buffer *
lab_img_render(struct lab_img *img, int width, int height, int padding,
	double scale)
{
	struct lab_data_buffer *buffer = NULL;

	/* Render the image into the buffer for the given size */
	switch (img->cache->type) {
	case LAB_IMG_PNG:
	case LAB_IMG_XBM:
	case LAB_IMG_XPM:
		buffer = render_cairo_surface(img->cache->buffer->surface,
			width, height, padding, scale);
		break;
#if HAVE_RSVG
	case LAB_IMG_SVG:
		buffer = img_svg_render(img->cache->svg, width, height,
			padding, scale);
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
		(*modifier)(img->theme, cairo, width, height);
		cairo_restore(cairo);
	}

	cairo_surface_flush(buffer->surface);
	cairo_destroy(cairo);

	return buffer;
}

static void
consider_destroy_img(struct lab_img *img)
{
	if (!img->dropped || img->nr_locks > 0) {
		return;
	}

	struct lab_img_cache *cache = img->cache;
	cache->refcount--;

	if (cache->refcount == 0) {
		if (cache->buffer) {
			wlr_buffer_drop(&cache->buffer->base);
		}
#if HAVE_RSVG
		if (cache->svg) {
			g_object_unref(cache->svg);
		}
#endif
		free(cache);
	}

	wl_array_release(&img->modifiers);
	free(img);
}

void
lab_img_lock(struct lab_img *img)
{
	assert(img);
	img->nr_locks++;
}

void
lab_img_unlock(struct lab_img *img)
{
	assert(img);
	img->nr_locks--;
	consider_destroy_img(img);
}

void
lab_img_drop(struct lab_img *img)
{
	if (!img) {
		return;
	}
	img->dropped = true;
	consider_destroy_img(img);
}
