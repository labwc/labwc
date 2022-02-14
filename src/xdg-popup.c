// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 the sway authors
 *
 * This file is only needed in support of
 *	- unconstraining XDG popups
 */

#include "labwc.h"

/* TODO: surely this ought to just move to xdg.c now??? */
static void
popup_unconstrain(struct view *view, struct wlr_xdg_popup *popup)
{
	struct server *server = view->server;
	struct wlr_box *popup_box = &popup->geometry;
	struct wlr_output_layout *output_layout = server->output_layout;
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		output_layout, view->x + popup_box->x, view->y + popup_box->y);

	struct wlr_box output_box;
	wlr_output_layout_get_box(output_layout, wlr_output, &output_box);

	struct wlr_box output_toplevel_box = {
		.x = output_box.x - view->x,
		.y = output_box.y - view->y,
		.width = output_box.width,
		.height = output_box.height,
	};
	wlr_xdg_popup_unconstrain_from_box(popup, &output_toplevel_box);
}

void
xdg_popup_create(struct view *view, struct wlr_xdg_popup *wlr_popup)
{
	popup_unconstrain(view, wlr_popup);
}
