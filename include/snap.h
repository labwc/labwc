/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SNAP_H
#define LABWC_SNAP_H

#include "common/border.h"
#include "view.h"

struct wlr_box;

void snap_move_to_edge(struct view *view,
	enum view_edge direction, bool snap_to_windows, int *dx, int *dy);

void snap_grow_to_next_edge(struct view *view,
	enum view_edge direction, struct wlr_box *geo);

void snap_shrink_to_next_edge(struct view *view,
	enum view_edge direction, struct wlr_box *geo);

void snap_invalidate_edge_cache(struct view *view);
void snap_update_cache_geometry(struct view *view);

#endif /* LABWC_SNAP_H */
