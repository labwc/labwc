/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OVERLAY_H
#define LABWC_OVERLAY_H

#include <wlr/util/box.h>
#include "common/graphic-helpers.h"

struct overlay {
	struct wlr_scene_tree *tree;
	union {
		struct wlr_scene_rect *rect;
		struct multi_rect *pixman_rect;
	};

	/* For delayed overlay */
	struct view *view;
	struct wlr_box box;
	struct wl_event_source *timer;
};

/* Calls overlay_hide() internally if the view is not to be snapped */
void overlay_show(struct seat *seat, struct view *view);
void overlay_hide(struct seat *seat);

#endif
