/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SNAP_H
#define LABWC_SNAP_H

#include "common/border.h"
#include "view.h"

struct wlr_box;

struct border snap_get_max_distance(struct view *view);

void snap_vector_to_next_edge(struct view *view, enum view_edge direction, int *dx, int *dy);
int snap_distance_to_next_edge(struct view *view, enum view_edge direction);
void snap_grow_to_next_edge(struct view *view, enum view_edge direction, struct wlr_box *geo);
void snap_shrink_to_next_edge(struct view *view, enum view_edge direction, struct wlr_box *geo);

#endif /* LABWC_SNAP_H */
