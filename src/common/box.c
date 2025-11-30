// SPDX-License-Identifier: GPL-2.0-only
#include "common/box.h"
#include "common/macros.h"

bool
box_intersects(struct wlr_box *box_a, struct wlr_box *box_b)
{
	if (wlr_box_empty(box_a) || wlr_box_empty(box_b)) {
		return false;
	}
	return box_a->x < box_b->x + box_b->width
		&& box_b->x < box_a->x + box_a->width
		&& box_a->y < box_b->y + box_b->height
		&& box_b->y < box_a->y + box_a->height;
}

void
box_union(struct wlr_box *box_dest, struct wlr_box *box_a, struct wlr_box *box_b)
{
	if (wlr_box_empty(box_a)) {
		*box_dest = *box_b;
		return;
	}
	if (wlr_box_empty(box_b)) {
		*box_dest = *box_a;
		return;
	}
	int x1 = MIN(box_a->x, box_b->x);
	int y1 = MIN(box_a->y, box_b->y);
	int x2 = MAX(box_a->x + box_a->width, box_b->x + box_b->width);
	int y2 = MAX(box_a->y + box_a->height, box_b->y + box_b->height);
	box_dest->x = x1;
	box_dest->y = y1;
	box_dest->width = x2 - x1;
	box_dest->height = y2 - y1;
}

void
box_center(int width, int height, const struct wlr_box *ref,
		const struct wlr_box *bound, int *x, int *y)
{
	*x = ref->x + (ref->width - width) / 2;
	*y = ref->y + (ref->height - height) / 2;

	if (*x < bound->x) {
		*x = bound->x;
	} else if (*x + width > bound->x + bound->width) {
		*x = bound->x + bound->width - width;
	}
	if (*y < bound->y) {
		*y = bound->y;
	} else if (*y + height > bound->y + bound->height) {
		*y = bound->y + bound->height - height;
	}
}

struct wlr_box
box_fit_within(int width, int height, struct wlr_box *bound)
{
	struct wlr_box box;

	if (width <= bound->width && height <= bound->height) {
		/* No downscaling needed */
		box.width = width;
		box.height = height;
	} else if (width * bound->height > height * bound->width) {
		/* Wider content, fit width */
		box.width = bound->width;
		box.height = (height * bound->width + (width / 2)) / width;
	} else {
		/* Taller content, fit height */
		box.width = (width * bound->height + (height / 2)) / height;
		box.height = bound->height;
	}

	/* Compute centered position */
	box.x = bound->x + (bound->width - box.width) / 2;
	box.y = bound->y + (bound->height - box.height) / 2;

	return box;
}

struct wlr_fbox
box_to_fbox(struct wlr_box *box)
{
	return (struct wlr_fbox){
		.x = box->x,
		.y = box->y,
		.width = box->width,
		.height = box->height,
	};
}
