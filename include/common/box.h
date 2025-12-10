/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BOX_H
#define LABWC_BOX_H

#include <wlr/util/box.h>

bool box_intersects(struct wlr_box *box_a, struct wlr_box *box_b);

/* Returns the bounding box of 2 boxes */
void box_union(struct wlr_box *box_dest, struct wlr_box *box_a,
	struct wlr_box *box_b);

/*
 * Centers a content box (width & height) within a reference box,
 * limiting it (if possible) to not extend outside a bounding box.
 *
 * The reference box and bounding box are often the same but could be
 * different (e.g. when centering a view within its parent but limiting
 * to usable output area).
 */
void box_center(int width, int height, const struct wlr_box *ref,
	const struct wlr_box *bound, int *x, int *y);

/*
 * Fits and centers a content box (width & height) within a bounding box.
 * The content box is downscaled if necessary (preserving aspect ratio) but
 * not upscaled.
 *
 * The returned x & y coordinates are the centered content position
 * relative to the top-left corner of the bounding box.
 */
struct wlr_box box_fit_within(int width, int height, struct wlr_box *bound);

struct wlr_fbox box_to_fbox(struct wlr_box *box);

#endif /* LABWC_BOX_H */
