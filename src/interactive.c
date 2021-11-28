// SPDX-License-Identifier: GPL-2.0-only
#include "labwc.h"

void
interactive_begin(struct view *view, enum input_mode mode, uint32_t edges)
{
	if (view->maximized) {
		return;
	}

	/*
	 * This function sets up an interactive move or resize operation, where
	 * the compositor stops propegating pointer events to clients and
	 * instead consumes them itself, to move or resize windows.
	 */
	struct seat *seat = &view->server->seat;
	struct server *server = view->server;
	server->grabbed_view = view;
	server->input_mode = mode;

	/* Remember view and cursor positions at start of move/resize */
	server->grab_x = seat->cursor->x;
	server->grab_y = seat->cursor->y;
	struct wlr_box box = {
		.x = view->x, .y = view->y, .width = view->w, .height = view->h
	};
	memcpy(&server->grab_box, &box, sizeof(struct wlr_box));
	server->resize_edges = edges;

	switch (mode) {
	case LAB_INPUT_STATE_MOVE:
		cursor_set(&server->seat, "move");
		break;
	case LAB_INPUT_STATE_RESIZE:
		cursor_set(&server->seat, wlr_xcursor_get_resize_name(edges));
		break;
	default:
		break;
	}
}
