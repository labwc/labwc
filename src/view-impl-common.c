// SPDX-License-Identifier: GPL-2.0-only
/* view-impl-common.c: common code for shell view->impl functions */
#include <stdio.h>
#include <strings.h>
#include "labwc.h"
#include "view.h"
#include "view-impl-common.h"

void
view_impl_move_to_front(struct view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
view_impl_map(struct view *view)
{
	desktop_focus_and_activate_view(&view->server->seat, view);
	desktop_move_to_front(view);
	view_update_title(view);
	view_update_app_id(view);
}
