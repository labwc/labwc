// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_output_layout.h>
#include "common/direction.h"
#include "view.h"

enum wlr_direction
direction_from_view_edge(enum view_edge edge)
{
	switch (edge) {
	case VIEW_EDGE_LEFT:
		return WLR_DIRECTION_LEFT;
	case VIEW_EDGE_RIGHT:
		return WLR_DIRECTION_RIGHT;
	case VIEW_EDGE_UP:
		return WLR_DIRECTION_UP;
	case VIEW_EDGE_DOWN:
		return WLR_DIRECTION_DOWN;
	case VIEW_EDGE_CENTER:
	case VIEW_EDGE_INVALID:
	default:
		return WLR_DIRECTION_UP;
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
