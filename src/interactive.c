#include "labwc.h"

void interactive_begin(struct view *view, enum cursor_mode mode, uint32_t edges)
{
	/*
	 * This function sets up an interactive move or resize operation, where
	 * the compositor stops propegating pointer events to clients and
	 * instead consumes them itself, to move or resize windows.
	 */
	struct server *server = view->server;
	server->grabbed_view = view;
	server->cursor_mode = mode;

	/* Remember view and cursor positions at start of move/resize */
	server->grab_x = server->cursor->x;
	server->grab_y = server->cursor->y;
	server->grab_box = view_geometry(view);
	server->resize_edges = edges;
}
