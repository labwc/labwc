/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OVERLAY_H
#define LABWC_OVERLAY_H

#include "common/edge.h"

struct seat;

struct overlay {
	struct lab_scene_rect *rect;

	/* Represents currently shown or delayed overlay */
	struct {
		/* Region overlay */
		struct region *region;

		/* Snap-to-edge overlay */
		enum lab_edge edge;
		struct output *output;
	} active;

	/* For delayed snap-to-edge overlay */
	struct wl_event_source *timer;
};

/*
 * Shows or updates an overlay when the grabbed window can be snapped to
 * a region or an output edge. Calls overlay_finish() otherwise.
 */
void overlay_update(struct seat *seat);

/* Destroys the overlay if it exists */
void overlay_finish(struct seat *seat);

#endif
