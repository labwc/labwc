/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_TOOL_H
#define LABWC_TABLET_TOOL_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_tablet_v2.h>

struct seat;

struct drawing_tablet_tool {
	struct seat *seat;
	struct wlr_tablet_v2_tablet_tool *tool_v2;
	struct {
		struct wl_listener set_cursor;
		struct wl_listener destroy;
	} handlers;
	struct wl_list link; /* seat.tablet_tools */
};

void tablet_tool_create(struct seat *seat,
	struct wlr_tablet_tool *wlr_tablet_tool);
bool tablet_tool_has_focused_surface(struct seat *seat);

#endif /* LABWC_TABLET_TOOL_H */
