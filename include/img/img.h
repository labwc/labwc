/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_H
#define LABWC_IMG_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>

enum lab_img_type {
	LAB_IMG_PNG,
	LAB_IMG_SVG,
	LAB_IMG_XBM,
	LAB_IMG_XPM,
};

struct lab_img {
	struct wl_array modifiers; /* lab_img_modifier_func_t */
	struct lab_img_data *data;
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

typedef void (*lab_img_modifier_func_t)(cairo_t *cairo, int w, int h);

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
void lab_img_add_modifier(struct lab_img *img, lab_img_modifier_func_t modifier);

/**
 * lab_img_render() - Render lab_img to a buffer
 * @img: source image
 * @width: width of the created buffer
 * @height: height of the created buffer
 * @padding_x: horizontal padding around the rendered image in the buffer
 * @scale: scale of the created buffer
 */
struct lab_data_buffer *lab_img_render(struct lab_img *img,
	int width, int height, int padding_x, double scale);

/**
 * lab_img_destroy() - destroy lab_img
 * @img: lab_img to destroy
 */
void lab_img_destroy(struct lab_img *img);

/**
 * lab_img_equal() - Returns true if two images draw the same content
 */
bool lab_img_equal(struct lab_img *img_a, struct lab_img *img_b);

#endif /* LABWC_IMG_H */
