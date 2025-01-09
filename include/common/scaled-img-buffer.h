/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCALED_IMG_BUFFER_H
#define LABWC_SCALED_IMG_BUFFER_H

#include <stdbool.h>

struct wlr_scene_tree;
struct wlr_scene_node;
struct wlr_scene_buffer;
struct lab_img;

struct scaled_img_buffer {
	struct scaled_scene_buffer *scaled_buffer;
	struct wlr_scene_buffer *scene_buffer;
	struct lab_img *img;
	int width;
	int height;
};

/*
 *                                                 |                 |
 *                                       .------------------.  .------------.
 *                   scaled_img_buffer   | new_output_scale |  | set_buffer |
 *                     architecture      ´------------------`  ´------------`
 *                                                 |                ^
 *                .--------------------------------|----------------|-------------.
 *                |                                v                |             |
 *                |  .-------------------.    .-------------------------.         |
 *                |  | scaled_img_buffer |----| wlr_buffer LRU cache(2) |<----,   |
 *                |  ´-------------------`    ´-------------------------`     |   |
 *                |            |                           |                  |   |
 *                |            |               .--------------------------.   |   |
 *                |            |               | wlr_buffer LRU cache of  |   |   |
 *   .-------.    |            |               | other scaled_img_buffers |   |   |
 *   | theme |    |            |               |   with lab_img_equal()   |   |   |
 *   ´-------`    |            |               ´--------------------------`   |   |
 *       |        |            |                  /              |            |   |
 *       |        |            |             not found         found          |   |
 *  .---------.   |        .---------.     .----------.    .------------.     |   |
 *  | lab_img |-img_copy-->| lab_img |-----| render() |--->| wlr_buffer |-----`   |
 *  ´---------`   |        ´---------`     ´----------`    ´------------`         |
 *           \    |           /                                                   |
 *            \   ´----------/----------------------------------------------------`
 *             \            /
 *           .----------------.                       lab_img provides:
 *           |  lab_img_data  |                       - render function
 *           |   refcount=2   |                       - list of modification functions
 *           |                `-----------------.       to apply on top of lib_img_data
 *           |                                  |       when rendering
 *           | provides (depending on backend): |     - lab_img_equal() comparing the
 *           | - librsvg handle                 |       lab_img_data reference and
 *           | - cairo surface                  |       modification function pointers
 *           ´----------------------------------`       of two given lab_img instances
 *
 */

/*
 * Create an auto scaling image buffer, providing a wlr_scene_buffer node for
 * display. It gets destroyed automatically when the backing scaled_scene_buffer
 * is being destroyed which in turn happens automatically when the backing
 * wlr_scene_buffer (or one of its parents) is being destroyed.
 *
 * This function clones the lab_img passed as the image source, so callers are
 * free to destroy it.
 */
struct scaled_img_buffer *scaled_img_buffer_create(struct wlr_scene_tree *parent,
	struct lab_img *img, int width, int height);

/* Update image, width and height of the scaled_img_buffer */
void scaled_img_buffer_update(struct scaled_img_buffer *self,
	struct lab_img *img, int width, int height);

/* Obtain scaled_img_buffer from wlr_scene_node */
struct scaled_img_buffer *scaled_img_buffer_from_node(struct wlr_scene_node *node);

#endif /* LABWC_SCALED_IMG_BUFFER_H */
