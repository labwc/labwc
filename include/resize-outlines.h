/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RESIZE_OUTLINES_H
#define LABWC_RESIZE_OUTLINES_H

#include <wlr/util/box.h>

struct view;

void resize_outlines_update(struct view *view, struct wlr_box new_geo);
void resize_outlines_finish(struct view *view);
bool resize_outlines_enabled(struct view *view);

#endif /* LABWC_RESIZE_OUTLINES_H */
