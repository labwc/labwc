// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "input/cursor.h"
#include "input/tablet-tool.h"
#include "labwc.h"

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_tool *tool =
		wl_container_of(listener, tool, handlers.destroy);

	wl_list_remove(&tool->handlers.set_cursor.link);
	wl_list_remove(&tool->handlers.destroy.link);
	free(tool);
}

void
tablet_tool_init(struct seat *seat,
		struct wlr_tablet_tool *wlr_tablet_tool)
{
	wlr_log(WLR_DEBUG, "setting up tablet tool");
	struct drawing_tablet_tool *tool = znew(*tool);
	tool->seat = seat;
	tool->tool_v2 =
		wlr_tablet_tool_create(seat->server->tablet_manager,
			seat->seat, wlr_tablet_tool);
	wlr_tablet_tool->data = tool;
	wlr_log(WLR_INFO, "tablet tool capabilities:%s%s%s%s%s%s",
		wlr_tablet_tool->tilt ? " tilt" : "",
		wlr_tablet_tool->pressure ? " pressure" : "",
		wlr_tablet_tool->distance ? " distance" : "",
		wlr_tablet_tool->rotation ? " rotation" : "",
		wlr_tablet_tool->slider ? " slider" : "",
		wlr_tablet_tool->wheel ? " wheel" : "");
	CONNECT_SIGNAL(wlr_tablet_tool, &tool->handlers, destroy);
}
