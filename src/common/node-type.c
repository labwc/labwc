// SPDX-License-Identifier: GPL-2.0-only
#include "common/node-type.h"
#include <strings.h>
#include <wlr/util/log.h>

enum lab_node_type
node_type_parse(const char *context)
{
	if (!strcasecmp(context, "Close")) {
		return LAB_NODE_BUTTON_CLOSE;
	} else if (!strcasecmp(context, "Maximize")) {
		return LAB_NODE_BUTTON_MAXIMIZE;
	} else if (!strcasecmp(context, "Iconify")) {
		return LAB_NODE_BUTTON_ICONIFY;
	} else if (!strcasecmp(context, "WindowMenu")) {
		return LAB_NODE_BUTTON_WINDOW_MENU;
	} else if (!strcasecmp(context, "Icon")) {
		return LAB_NODE_BUTTON_WINDOW_ICON;
	} else if (!strcasecmp(context, "Shade")) {
		return LAB_NODE_BUTTON_SHADE;
	} else if (!strcasecmp(context, "AllDesktops")) {
		return LAB_NODE_BUTTON_OMNIPRESENT;
	} else if (!strcasecmp(context, "Titlebar")) {
		return LAB_NODE_TITLEBAR;
	} else if (!strcasecmp(context, "Title")) {
		return LAB_NODE_TITLE;
	} else if (!strcasecmp(context, "TLCorner")) {
		return LAB_NODE_CORNER_TOP_LEFT;
	} else if (!strcasecmp(context, "TRCorner")) {
		return LAB_NODE_CORNER_TOP_RIGHT;
	} else if (!strcasecmp(context, "BRCorner")) {
		return LAB_NODE_CORNER_BOTTOM_RIGHT;
	} else if (!strcasecmp(context, "BLCorner")) {
		return LAB_NODE_CORNER_BOTTOM_LEFT;
	} else if (!strcasecmp(context, "Border")) {
		return LAB_NODE_BORDER;
	} else if (!strcasecmp(context, "Top")) {
		return LAB_NODE_BORDER_TOP;
	} else if (!strcasecmp(context, "Right")) {
		return LAB_NODE_BORDER_RIGHT;
	} else if (!strcasecmp(context, "Bottom")) {
		return LAB_NODE_BORDER_BOTTOM;
	} else if (!strcasecmp(context, "Left")) {
		return LAB_NODE_BORDER_LEFT;
	} else if (!strcasecmp(context, "Frame")) {
		return LAB_NODE_FRAME;
	} else if (!strcasecmp(context, "Client")) {
		return LAB_NODE_CLIENT;
	} else if (!strcasecmp(context, "Desktop")) {
		return LAB_NODE_ROOT;
	} else if (!strcasecmp(context, "Root")) {
		return LAB_NODE_ROOT;
	} else if (!strcasecmp(context, "All")) {
		return LAB_NODE_ALL;
	}
	wlr_log(WLR_ERROR, "unknown mouse context (%s)", context);
	return LAB_NODE_NONE;
}

bool
node_type_contains(enum lab_node_type whole, enum lab_node_type part)
{
	if (whole == part || whole == LAB_NODE_ALL) {
		return true;
	}
	if (whole == LAB_NODE_BUTTON) {
		return part >= LAB_NODE_BUTTON_FIRST
			&& part <= LAB_NODE_BUTTON_LAST;
	}
	if (whole == LAB_NODE_TITLEBAR) {
		return part >= LAB_NODE_BUTTON_FIRST
			&& part <= LAB_NODE_TITLE;
	}
	if (whole == LAB_NODE_TITLE) {
		/* "Title" includes blank areas of "Titlebar" as well */
		return part >= LAB_NODE_TITLEBAR
			&& part <= LAB_NODE_TITLE;
	}
	if (whole == LAB_NODE_FRAME) {
		return part >= LAB_NODE_BUTTON_FIRST
			&& part <= LAB_NODE_CLIENT;
	}
	if (whole == LAB_NODE_BORDER) {
		return part >= LAB_NODE_CORNER_TOP_LEFT
			&& part <= LAB_NODE_BORDER_LEFT;
	}
	if (whole == LAB_NODE_BORDER_TOP) {
		return part == LAB_NODE_CORNER_TOP_LEFT
			|| part == LAB_NODE_CORNER_TOP_RIGHT;
	}
	if (whole == LAB_NODE_BORDER_RIGHT) {
		return part == LAB_NODE_CORNER_TOP_RIGHT
			|| part == LAB_NODE_CORNER_BOTTOM_RIGHT;
	}
	if (whole == LAB_NODE_BORDER_BOTTOM) {
		return part == LAB_NODE_CORNER_BOTTOM_RIGHT
			|| part == LAB_NODE_CORNER_BOTTOM_LEFT;
	}
	if (whole == LAB_NODE_BORDER_LEFT) {
		return part == LAB_NODE_CORNER_TOP_LEFT
			|| part == LAB_NODE_CORNER_BOTTOM_LEFT;
	}
	return false;
}

enum lab_edge
node_type_to_edges(enum lab_node_type type)
{
	switch (type) {
	case LAB_NODE_BORDER_TOP:
		return LAB_EDGE_TOP;
	case LAB_NODE_BORDER_RIGHT:
		return LAB_EDGE_RIGHT;
	case LAB_NODE_BORDER_BOTTOM:
		return LAB_EDGE_BOTTOM;
	case LAB_NODE_BORDER_LEFT:
		return LAB_EDGE_LEFT;
	case LAB_NODE_CORNER_TOP_LEFT:
		return LAB_EDGES_TOP_LEFT;
	case LAB_NODE_CORNER_TOP_RIGHT:
		return LAB_EDGES_TOP_RIGHT;
	case LAB_NODE_CORNER_BOTTOM_RIGHT:
		return LAB_EDGES_BOTTOM_RIGHT;
	case LAB_NODE_CORNER_BOTTOM_LEFT:
		return LAB_EDGES_BOTTOM_LEFT;
	default:
		return LAB_EDGE_NONE;
	}
}
