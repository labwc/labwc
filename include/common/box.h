/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BOX_H
#define LABWC_BOX_H

#include <wlr/util/box.h>

bool box_contains(struct wlr_box *box_super, struct wlr_box *box_sub);

bool box_intersects(struct wlr_box *box_a, struct wlr_box *box_b);

/* Returns the bounding box of 2 boxes */
void box_union(struct wlr_box *box_dest, struct wlr_box *box_a,
	struct wlr_box *box_b);

/*
 * Fits and centers a content box (width & height) within a bounding box
 * (max_width & max_height). The content box is downscaled if necessary
 * (preserving aspect ratio) but not upscaled.
 *
 * The returned x & y coordinates are the centered content position
 * relative to the top-left corner of the bounding box.
 */
struct wlr_box box_fit_within(int width, int height, int max_width,
	int max_height);

#endif /* LABWC_BOX_H */
