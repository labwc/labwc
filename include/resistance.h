/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RESISTANCE_H
#define LABWC_RESISTANCE_H
#include "labwc.h"

/**
 * resistance_unsnap_apply() - Apply resistance when dragging a
 * maximized/tiled window. When the view is maximized/tiled and it needs to be
 * un-tiled, new_view_geo->{width,height} are set with the new geometry.
 * Otherwise, new_view_geo->{width,height} are untouched.
 */
void resistance_unsnap_apply(struct view *view, struct wlr_box *new_view_geo);
void resistance_move_apply(struct view *view, int *x, int *y);
void resistance_resize_apply(struct view *view, struct wlr_box *new_view_geo);

#endif /* LABWC_RESISTANCE_H */
