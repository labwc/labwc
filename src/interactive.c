// SPDX-License-Identifier: GPL-2.0-only
#include "labwc.h"

static int
max_move_scale(double pos_cursor, double pos_current,
	double size_current, double size_orig)
{
	double anchor_frac = (pos_cursor - pos_current) / size_current;
	int pos_new = pos_cursor - (size_orig * anchor_frac);
	if (pos_new < pos_current) {
		/* Clamp by using the old offsets of the maximized window */
		pos_new = pos_current;
	}
	return pos_new;
}

void
interactive_begin(struct view *view, enum input_mode mode, uint32_t edges)
{
	if (view->maximized) {
		if (mode == LAB_INPUT_STATE_MOVE) {
			int new_x = max_move_scale(view->server->seat.cursor->x,
				view->x, view->w, view->unmaximized_geometry.width);
			int new_y = max_move_scale(view->server->seat.cursor->y,
				view->y, view->h, view->unmaximized_geometry.height);
			view->unmaximized_geometry.x = new_x;
			view->unmaximized_geometry.y = new_y;
			view_maximize(view, false);
			/*
			 * view_maximize() indirectly calls view->impl->configure
			 * which is async but we are using the current values in
			 * server->grab_box. We pretend the configure already
			 * happened by setting them manually.
			 */
			view->x = new_x;
			view->y = new_y;
			view->w = view->unmaximized_geometry.width;
			view->h = view->unmaximized_geometry.height;
		} else {
			return;
		}
	}

	/*
	 * This function sets up an interactive move or resize operation, where
	 * the compositor stops propagating pointer events to clients and
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
		cursor_set(&server->seat, "grab");
		break;
	case LAB_INPUT_STATE_RESIZE:
		cursor_set(&server->seat, wlr_xcursor_get_resize_name(edges));
		break;
	default:
		break;
	}
}

void
interactive_end(struct view *view)
{
	if (view->server->grabbed_view == view) {
		view->server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
		view->server->grabbed_view = NULL;
	}
}
