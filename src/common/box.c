// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include "common/box.h"
#include "common/macros.h"

bool
box_contains(struct wlr_box *box_super, struct wlr_box *box_sub)
{
	if (wlr_box_empty(box_super) || wlr_box_empty(box_sub)) {
		return false;
	}
	return box_super->x <= box_sub->x
		&& box_super->x + box_super->width >= box_sub->x + box_sub->width
		&& box_super->y <= box_sub->y
		&& box_super->y + box_super->height >= box_sub->y + box_sub->height;
}

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
