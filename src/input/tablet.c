// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <linux/input-event-codes.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "input/cursor.h"
#include "input/tablet.h"

static void
handle_axis(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_axis_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	if (ev->updated_axes & (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) {
		if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_X) {
			tablet->x = ev->x;
		}
		if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_Y) {
			tablet->y = ev->y;
		}

		double x = tablet->x;
		double y = tablet->y;
		cursor_emulate_move_absolute(tablet->seat, &ev->tablet->base, x, y, ev->time_msec);
	}
	// Ignore other events
}

static uint32_t
get_mapped_button(uint32_t src_button)
{
	struct button_map_entry *map_entry;
	for (size_t i = 0; i < rc.tablet.button_map_count; i++) {
		map_entry = &rc.tablet.button_map[i];
		if (map_entry->from == src_button) {
			return map_entry->to;
		}
	}
	wlr_log(WLR_DEBUG, "no button map target for 0x%x", src_button);
	return 0;
}

static void
handle_tip(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_tip_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;

	uint32_t button = get_mapped_button(BTN_TOOL_PEN);
	if (!button) {
		return;
	}

	cursor_emulate_button(tablet->seat,
		button,
		ev->state == WLR_TABLET_TOOL_TIP_DOWN
			? WLR_BUTTON_PRESSED
			: WLR_BUTTON_RELEASED,
		ev->time_msec);
}

static void
handle_button(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_button_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;

	uint32_t button = get_mapped_button(ev->button);
	if (!button) {
		return;
	}

	cursor_emulate_button(tablet->seat, button, ev->state, ev->time_msec);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet *tablet =
		wl_container_of(listener, tablet, handlers.destroy);
	free(tablet);
}

static void
setup_pad(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_INFO, "not setting up pad");
}

static void
setup_pen(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_DEBUG, "setting up tablet");
	struct drawing_tablet *tablet = znew(*tablet);
	tablet->seat = seat;
	tablet->tablet = wlr_tablet_from_input_device(wlr_device);
	tablet->tablet->data = tablet;
	tablet->x = 0.0;
	tablet->y = 0.0;
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, axis);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, tip);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, button);
	CONNECT_SIGNAL(wlr_device, &tablet->handlers, destroy);
}

void
tablet_setup_handlers(struct seat *seat, struct wlr_input_device *device)
{
	switch (device->type) {
	case WLR_INPUT_DEVICE_TABLET_PAD:
		setup_pad(seat, device);
		break;
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		setup_pen(seat, device);
		break;
	default:
		assert(false && "tried to add non-tablet as tablet");
	}
}
