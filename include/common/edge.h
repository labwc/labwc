/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_EDGE_H
#define LABWC_EDGE_H

#include <wayland-server-core.h>

/**
 * Represents an edge or direction (e.g. window tiling, window motion)
 */
enum lab_edge {
	LAB_EDGE_INVALID = 0,

	LAB_EDGE_LEFT = (1 << 0),
	LAB_EDGE_RIGHT = (1 << 1),
	LAB_EDGE_UP = (1 << 2),
	LAB_EDGE_DOWN = (1 << 3),
	LAB_EDGE_CENTER = (1 << 4), /* for window tiling */
	LAB_EDGE_ANY = (1 << 5), /* for window rules */

	/* for window tiling */
	LAB_EDGE_UPLEFT = (LAB_EDGE_UP | LAB_EDGE_LEFT),
	LAB_EDGE_UPRIGHT = (LAB_EDGE_UP | LAB_EDGE_RIGHT),
	LAB_EDGE_DOWNLEFT = (LAB_EDGE_DOWN | LAB_EDGE_LEFT),
	LAB_EDGE_DOWNRIGHT = (LAB_EDGE_DOWN | LAB_EDGE_RIGHT),
};

enum lab_edge lab_edge_parse(const char *direction, bool tiled, bool any);

/**
 * lab_edge_invert() - select the opposite of a provided edge
 *
 * Returns LAB_EDGE_INVALID for edges other than UP/DOWN/LEFT/RIGHT.
 *
 * @edge: edge to be inverted
 */
enum lab_edge lab_edge_invert(enum lab_edge edge);

#endif /* LABWC_EDGE_H */
