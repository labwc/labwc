// SPDX-License-Identifier: GPL-2.0-only
#include "common/edge.h"
#include <strings.h>

enum lab_edge
lab_edge_parse(const char *direction, bool tiled, bool any)
{
	if (!direction) {
		return LAB_EDGE_INVALID;
	}
	if (!strcasecmp(direction, "left")) {
		return LAB_EDGE_LEFT;
	} else if (!strcasecmp(direction, "up")) {
		return LAB_EDGE_UP;
	} else if (!strcasecmp(direction, "right")) {
		return LAB_EDGE_RIGHT;
	} else if (!strcasecmp(direction, "down")) {
		return LAB_EDGE_DOWN;
	}

	if (any) {
		if (!strcasecmp(direction, "any")) {
			return LAB_EDGE_ANY;
		}
	}

	if (tiled) {
		if (!strcasecmp(direction, "center")) {
			return LAB_EDGE_CENTER;
		} else if (!strcasecmp(direction, "up-left")) {
			return LAB_EDGE_UPLEFT;
		} else if (!strcasecmp(direction, "up-right")) {
			return LAB_EDGE_UPRIGHT;
		} else if (!strcasecmp(direction, "down-left")) {
			return LAB_EDGE_DOWNLEFT;
		} else if (!strcasecmp(direction, "down-right")) {
			return LAB_EDGE_DOWNRIGHT;
		}
	}

	return LAB_EDGE_INVALID;
}

enum lab_edge
lab_edge_invert(enum lab_edge edge)
{
	switch (edge) {
	case LAB_EDGE_LEFT:
		return LAB_EDGE_RIGHT;
	case LAB_EDGE_RIGHT:
		return LAB_EDGE_LEFT;
	case LAB_EDGE_UP:
		return LAB_EDGE_DOWN;
	case LAB_EDGE_DOWN:
		return LAB_EDGE_UP;
	default:
		return LAB_EDGE_INVALID;
	}
}
