/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OVERLAY_H
#define LABWC_OVERLAY_H

#include <wlr/util/box.h>
#include "common/graphic-helpers.h"
#include "regions.h"
#include "view.h"

struct overlay {
	struct wlr_scene_tree *tree;
	union {
		struct wlr_scene_rect *rect;
		struct multi_rect *pixman_rect;
	};

	/* For caching previously shown overlay */
	struct {
		struct region *region;
		enum view_edge edge;
	} active;

	/* For delayed snap-to-edge overlay */
	struct wl_event_source *timer;
	struct {
		struct view *view;
		struct wlr_box box;
	} pending;
};

/* Calls overlay_hide() internally if the view is not to be snapped */
void overlay_show(struct seat *seat, struct view *view);
/* This function must be called when grabbed view is destroyed */
void overlay_hide(struct seat *seat);

#endif
