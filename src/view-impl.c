// SPDX-License-Identifier: GPL-2.0-only
/* view-impl.c: common code for shell view->impl functions */
#include <stdio.h>
#include <strings.h>
#include "labwc.h"

void
view_impl_map(struct view *view)
{
	desktop_focus_and_activate_view(&view->server->seat, view);
	desktop_raise_view(view);

	const char *app_id = view->impl->get_string_prop(view, "app_id");
	if (app_id) {
		wlr_foreign_toplevel_handle_v1_set_app_id(
			view->toplevel_handle, app_id);
	}

	view_update_title(view);

	damage_all_outputs(view->server);
}
