/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCALED_RECT_BUFFER_H
#define LABWC_SCALED_RECT_BUFFER_H

#include <stdint.h>
#include <wlr/types/wlr_output_layout.h>
#include "common/corner.h"

struct wlr_scene_tree;
struct wlr_scene_buffer;
struct scaled_scene_buffer;

#define EDGES_ALL (WLR_DIRECTION_UP| WLR_DIRECTION_DOWN \
	| WLR_DIRECTION_LEFT | WLR_DIRECTION_RIGHT)

struct scaled_rect_buffer {
	struct wlr_scene_buffer *scene_buffer;
	struct scaled_scene_buffer *scaled_buffer;
	int width;
	int height;
	int border_width;
	int corner_radius;
	uint32_t rounded_corners; /* bitmap of lab_corner */
	uint32_t stroked_edges; /* bitmap of wlr_direction */
	float fill_color[4];
	float border_color[4];
};

/**
 * Create an auto scaling rounded-rect buffer, providing a wlr_scene_buffer
 * node for display. It gets destroyed automatically when the backing
 * scaled_scene_buffer is being destroyed which in turn happens automatically
 * when the backing wlr_scene_buffer (or one of its parents) is being destroyed.
 */
struct scaled_rect_buffer *scaled_rect_buffer_create(
	struct wlr_scene_tree *parent, int width, int height, int border_width,
	int corner_radius, uint32_t rounded_corners, uint32_t stroked_edges,
	float fill_color[4], float border_color[4]);

#endif /* LABWC_SCALED_RECT_BUFFER_H */
