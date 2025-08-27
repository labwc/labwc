/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_EDGE_H
#define LABWC_EDGE_H

#include <stdbool.h>

/**
 * Unified/overloaded enum representing edges, corners, and directions.
 * Used in many different contexts (moving, resizing, tiling) and with
 * somewhat different semantics depending on context.
 *
 * Examples:
 *   - LAB_EDGE_TOP can also mean "up" or "north".
 *   - LAB_EDGES_TOP_LEFT can mean "top left corner" or "northwest".
 *
 * The enum is designed to be used as a bitset, and combinations of
 * edges typically mean what you'd expect from the context. For example,
 * LAB_EDGES_TOP_LEFT is used when resizing a view from its top-left
 * corner, or when tiling a view in the top-left corner of an output.
 *
 * All 16 possible combinations of TOP/BOTTOM/LEFT/RIGHT are listed for
 * completeness. Not all combinations make sense in all contexts.
 *
 * LAB_EDGE_NONE is sometimes used to mean "invalid".
 *
 * LAB_EDGE_ANY means "any edge or combination of edges (except NONE)"
 * and is distinct from LAB_EDGE_ALL (which means all 4 edges).
 *
 * LAB_EDGE_TOP/BOTTOM/LEFT/RIGHT match the corresponding values of
 * enum wlr_edges and enum wlr_direction, so that conversion between
 * enums can be done with a simple cast.
 */
enum lab_edge {
	LAB_EDGE_NONE = 0,

	LAB_EDGE_TOP = (1 << 0),    /* or UP */
	LAB_EDGE_BOTTOM = (1 << 1), /* or DOWN */
	LAB_EDGE_LEFT = (1 << 2),
	LAB_EDGE_RIGHT = (1 << 3),
	LAB_EDGE_CENTER = (1 << 4), /* for window tiling */
	LAB_EDGE_ANY = (1 << 5), /* for window rules */

	/* corners or ordinal directions (NW/NE/SW/SE) */
	LAB_EDGES_TOP_LEFT = (LAB_EDGE_TOP | LAB_EDGE_LEFT),
	LAB_EDGES_TOP_RIGHT = (LAB_EDGE_TOP | LAB_EDGE_RIGHT),
	LAB_EDGES_BOTTOM_LEFT = (LAB_EDGE_BOTTOM | LAB_EDGE_LEFT),
	LAB_EDGES_BOTTOM_RIGHT = (LAB_EDGE_BOTTOM | LAB_EDGE_RIGHT),

	/* opposite edges */
	LAB_EDGES_TOP_BOTTOM = (LAB_EDGE_TOP | LAB_EDGE_BOTTOM),
	LAB_EDGES_LEFT_RIGHT = (LAB_EDGE_LEFT | LAB_EDGE_RIGHT),

	/* all 4 edges */
	LAB_EDGES_ALL = (LAB_EDGE_TOP | LAB_EDGE_BOTTOM |
			LAB_EDGE_LEFT | LAB_EDGE_RIGHT),

	/* 3-edge combinations (for completeness) */
	LAB_EDGES_EXCEPT_TOP = (LAB_EDGES_ALL ^ LAB_EDGE_TOP),
	LAB_EDGES_EXCEPT_BOTTOM = (LAB_EDGES_ALL ^ LAB_EDGE_BOTTOM),
	LAB_EDGES_EXCEPT_LEFT = (LAB_EDGES_ALL ^ LAB_EDGE_LEFT),
	LAB_EDGES_EXCEPT_RIGHT = (LAB_EDGES_ALL ^ LAB_EDGE_RIGHT),
};

enum lab_edge lab_edge_parse(const char *direction, bool tiled, bool any);

/**
 * Returns true if edge is TOP, BOTTOM, LEFT, or RIGHT
 * (i.e. one of the four cardinal directions N/S/W/E)
 */
bool lab_edge_is_cardinal(enum lab_edge edge);

/**
 * lab_edge_invert() - select the opposite of a provided edge
 *
 * Returns LAB_EDGE_NONE for edges other than TOP/BOTTOM/LEFT/RIGHT.
 *
 * @edge: edge to be inverted
 */
enum lab_edge lab_edge_invert(enum lab_edge edge);

#endif /* LABWC_EDGE_H */
