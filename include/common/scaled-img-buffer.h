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
	int padding;
};

/*
 * Create an auto scaling image buffer, providing a wlr_scene_buffer node for
 * display. It gets destroyed automatically when the backing scaled_scene_buffer
 * is being destroyed which in turn happens automatically when the backing
 * wlr_scene_buffer (or one of its parents) is being destroyed.
 */
struct scaled_img_buffer *scaled_img_buffer_create(struct wlr_scene_tree *parent,
	struct lab_img *img, int width, int height, int padding);

/* Update image, width, height and padding of the scaled_img_buffer */
void scaled_img_buffer_update(struct scaled_img_buffer *self,
	struct lab_img *img, int width, int height, int padding);

/* Obtain scaled_img_buffer from wlr_scene_node */
struct scaled_img_buffer *scaled_img_buffer_from_node(struct wlr_scene_node *node);

#endif /* LABWC_SCALED_IMG_BUFFER_H */
