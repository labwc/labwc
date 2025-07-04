// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_output_layout.h>
#include "common/direction.h"
#include "view.h"

bool
direction_from_view_edge(enum view_edge edge, enum wlr_direction *dir)
{
	switch (edge) {
	case VIEW_EDGE_LEFT:
		*dir = WLR_DIRECTION_LEFT;
		return true;
	case VIEW_EDGE_RIGHT:
		*dir = WLR_DIRECTION_RIGHT;
		return true;
	case VIEW_EDGE_UP:
		*dir = WLR_DIRECTION_UP;
		return true;
	case VIEW_EDGE_DOWN:
		*dir = WLR_DIRECTION_DOWN;
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
