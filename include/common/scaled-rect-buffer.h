/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCALED_RECT_BUFFER_H
#define LABWC_SCALED_RECT_BUFFER_H

#include <stdint.h>

struct wlr_scene_tree;
struct wlr_scene_buffer;
struct scaled_scene_buffer;

struct scaled_rect_buffer {
	struct wlr_scene_buffer *scene_buffer;
	struct scaled_scene_buffer *scaled_buffer;
	int width;
	int height;
	int border_width;
	float fill_color[4];
	float border_color[4];
};

/*
 * Create an auto scaling borderd-rectangle buffer, providing a wlr_scene_buffer
 * node for display. It gets destroyed automatically when the backing
 * scaled_scene_buffer is being destroyed which in turn happens automatically
 * when the backing wlr_scene_buffer (or one of its parents) is being destroyed.
 */
struct scaled_rect_buffer *scaled_rect_buffer_create(
	struct wlr_scene_tree *parent, int width, int height, int border_width,
	float fill_color[4], float border_color[4]);

#endif /* LABWC_SCALED_RECT_BUFFER_H */
