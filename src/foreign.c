#include "labwc.h"

void
foreign_toplevel_handle_create(struct view *view)
{
	view->toplevel_handle = wlr_foreign_toplevel_handle_v1_create(
		view->server->foreign_toplevel_manager);
	view_update_title(view);
	wlr_foreign_toplevel_handle_v1_output_enter(view->toplevel_handle,
		view_wlr_output(view));
}
