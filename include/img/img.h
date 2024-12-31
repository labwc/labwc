/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_H
#define LABWC_IMG_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>

struct lab_img_cache;

enum lab_img_type {
	LAB_IMG_PNG,
	LAB_IMG_SVG,
	LAB_IMG_XBM,
	LAB_IMG_XPM,
};

struct lab_img {
	struct theme *theme; /* Used by modifier functions */
	struct wl_array modifiers; /* lab_img_modifier_func_t */
	struct lab_img_cache *cache;

	bool dropped;
	int nr_locks;
};

struct lab_img *lab_img_load(enum lab_img_type type, const char *path,
	float *xbm_color);

/**
 * lab_img_load_from_bitmap() - create button from monochrome bitmap
 * @bitmap: bitmap data array in hexadecimal xbm format
 * @rgba: color
 *
 * Example bitmap: char button[6] = { 0x3f, 0x3f, 0x21, 0x21, 0x21, 0x3f };
 */
struct lab_img *lab_img_load_from_bitmap(const char *bitmap, float *rgba);

typedef void (*lab_img_modifier_func_t)(struct theme *theme, cairo_t *cairo,
	int w, int h);

/**
 * lab_img_copy() - Copy lab_img
 * @img: source image
 *
 * This function duplicates lab_img, but its internal cache for the image is
 * shared.
 */
struct lab_img *lab_img_copy(struct lab_img *img);

/**
 * lab_img_add_modifier() - Add a modifier function to lab_img
 * @img: source image
 * @modifier: function that applies modifications to the image.
 * @theme: pointer to theme passed to @modifier.
 *
 * "Modifiers" are functions that perform some additional drawing operation
 * after the image is rendered on a buffer with lab_img_render(). For example,
 * hover effects for window buttons can be drawn over the rendered image.
 */
void lab_img_add_modifier(struct lab_img *img, lab_img_modifier_func_t modifier,
	struct theme *theme);

/**
 * lab_img_render() - Render lab_img to a buffer
 * @img: source image
 * @width: width of the created buffer
 * @height: height of the created buffer
 * @padding: padding around the rendered image in the buffer
 * @scale: scale of the created buffer
 */
struct lab_data_buffer *lab_img_render(struct lab_img *img,
	int width, int height, int padding, double scale);

/* These functions closely follow the APIs of wlr_buffer */
void lab_img_lock(struct lab_img *img);
void lab_img_unlock(struct lab_img *img);
void lab_img_drop(struct lab_img *img);

#endif /* LABWC_IMG_H */
