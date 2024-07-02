/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BOX_H
#define LABWC_BOX_H

#include <wlr/util/box.h>

bool
box_contains(struct wlr_box *box_super, struct wlr_box *box_sub);

bool
box_intersects(struct wlr_box *box_a, struct wlr_box *box_b);

/* Returns the bounding box of 2 boxes */
void
box_union(struct wlr_box *box_dest, struct wlr_box *box_a, struct wlr_box *box_b);

#endif /* LABWC_BOX_H */
