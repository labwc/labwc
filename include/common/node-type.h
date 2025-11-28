/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_NODE_TYPE_H
#define LABWC_NODE_TYPE_H

#include "common/edge.h"

/*
 * In labwc, "node type" indicates the role of a wlr_scene_node in the
 * overall desktop. It also maps more-or-less to the openbox concept of
 * "context" (as used when defining mouse bindings).
 *
 * Some types (BUTTON, BORDER, etc.) refer to groups or categories of
 * other node types rather than individual nodes. These categories are
 * defined as ranges (e.g. BUTTON means anything from BUTTON_FIRST to
 * BUTTON_LAST), therefore order is significant.
 *
 * Be sure to keep node_type_contains() in sync with any changes!
 */
enum lab_node_type {
	LAB_NODE_NONE = 0,

	LAB_NODE_BUTTON_CLOSE = 1,
	LAB_NODE_BUTTON_MAXIMIZE,
	LAB_NODE_BUTTON_ICONIFY,
	LAB_NODE_BUTTON_WINDOW_ICON,
	LAB_NODE_BUTTON_WINDOW_MENU,
	LAB_NODE_BUTTON_SHADE,
	LAB_NODE_BUTTON_OMNIPRESENT,
	LAB_NODE_BUTTON_FIRST = LAB_NODE_BUTTON_CLOSE,
	LAB_NODE_BUTTON_LAST = LAB_NODE_BUTTON_OMNIPRESENT,
	LAB_NODE_BUTTON,

	LAB_NODE_TITLEBAR,
	LAB_NODE_TITLE,

	LAB_NODE_CORNER_TOP_LEFT,
	LAB_NODE_CORNER_TOP_RIGHT,
	LAB_NODE_CORNER_BOTTOM_RIGHT,
	LAB_NODE_CORNER_BOTTOM_LEFT,
	LAB_NODE_BORDER_TOP,
	LAB_NODE_BORDER_RIGHT,
	LAB_NODE_BORDER_BOTTOM,
	LAB_NODE_BORDER_LEFT,
	LAB_NODE_BORDER,

	LAB_NODE_CLIENT,
	LAB_NODE_FRAME,
	LAB_NODE_ROOT,
	LAB_NODE_MENUITEM,
	LAB_NODE_CYCLE_OSD_ITEM,
	LAB_NODE_LAYER_SURFACE,
	LAB_NODE_UNMANAGED,
	LAB_NODE_ALL,

	/* translated to LAB_NODE_CLIENT by get_cursor_context() */
	LAB_NODE_VIEW,
	LAB_NODE_XDG_POPUP,
	LAB_NODE_LAYER_POPUP,
	LAB_NODE_SESSION_LOCK_SURFACE,
	LAB_NODE_IME_POPUP,

	/*
	 * translated to LAB_CORNER_* or LAB_BORDER* by
	 * ssd_get_resizing_type()
	 */
	LAB_NODE_SSD_ROOT,
};

enum lab_node_type node_type_parse(const char *context);

bool node_type_contains(enum lab_node_type whole, enum lab_node_type part);

enum lab_edge node_type_to_edges(enum lab_node_type type);

#endif /* LABWC_NODE_TYPE_H */
