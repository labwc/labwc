/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RESISTANCE_H
#define LABWC_RESISTANCE_H
#include "labwc.h"

/**
 * resistance_unsnap_apply() - Apply resistance when dragging a
 * maximized/tiled window. Returns true when the view needs to be un-tiled.
 */
bool resistance_unsnap_apply(struct view *view, int *x, int *y);
void resistance_move_apply(struct view *view, int *x, int *y);
void resistance_resize_apply(struct view *view, struct wlr_box *new_view_geo);

#endif /* LABWC_RESISTANCE_H */
