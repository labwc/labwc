// SPDX-License-Identifier: GPL-2.0-only
#include "common/direction.h"
#include <assert.h>
#include <wlr/types/wlr_output_layout.h>
#include "view.h"

bool
direction_from_edge(enum lab_edge edge, enum wlr_direction *direction)
{
	switch (edge) {
	case LAB_EDGE_LEFT:
		*direction = WLR_DIRECTION_LEFT;
		return true;
	case LAB_EDGE_RIGHT:
		*direction = WLR_DIRECTION_RIGHT;
		return true;
	case LAB_EDGE_UP:
		*direction = WLR_DIRECTION_UP;
		return true;
	case LAB_EDGE_DOWN:
		*direction = WLR_DIRECTION_DOWN;
		return true;
	default:
		return false;
	}
}

enum wlr_direction
direction_get_opposite(enum wlr_direction direction)
{
	switch (direction) {
	case WLR_DIRECTION_RIGHT:
		return WLR_DIRECTION_LEFT;
	case WLR_DIRECTION_LEFT:
		return WLR_DIRECTION_RIGHT;
	case WLR_DIRECTION_DOWN:
		return WLR_DIRECTION_UP;
	case WLR_DIRECTION_UP:
		return WLR_DIRECTION_DOWN;
	default:
		assert(0); /* Unreachable */
		return WLR_DIRECTION_UP;
	}
}
