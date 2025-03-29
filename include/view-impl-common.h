/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_VIEW_IMPL_COMMON_H
#define LABWC_VIEW_IMPL_COMMON_H
/*
 * Common code for view->impl functions
 *
 * Please note: only xdg-shell-toplevel-view and xwayland-view view_impl
 * functions should call these functions.
 */

struct view;

void view_impl_map(struct view *view);
void view_impl_unmap(struct view *view);

/*
 * Updates view geometry at commit based on current position/size,
 * pending move/resize, and committed surface size. The computed
 * position may not match pending.x/y exactly in some cases.
 */
void view_impl_apply_geometry(struct view *view, int w, int h);

#endif /* LABWC_VIEW_IMPL_COMMON_H */
