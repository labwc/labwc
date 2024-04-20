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
};

void tablet_tool_init(struct seat *seat,
	struct wlr_tablet_tool *wlr_tablet_tool);

#endif /* LABWC_TABLET_TOOL_H */
