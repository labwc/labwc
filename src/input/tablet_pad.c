// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "input/cursor.h"
#include "input/tablet_pad.h"

static void
handle_button(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_pad *tablet_pad =
		wl_container_of(listener, tablet_pad, handlers.button);
	struct wlr_tablet_pad_button_event *ev = data;

	uint32_t button = tablet_get_mapped_button(ev->button);
	if (!button) {
		return;
	}

	cursor_emulate_button(tablet_pad->seat, button, ev->state, ev->time_msec);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_pad *tablet =
		wl_container_of(listener, tablet, handlers.destroy);
	free(tablet);
}

void
tablet_pad_init(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_DEBUG, "setting up tablet pad");
	struct drawing_tablet_pad *tablet = znew(*tablet);
	tablet->seat = seat;
	tablet->tablet = wlr_tablet_pad_from_input_device(wlr_device);
	tablet->tablet->data = tablet;
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, button);
	CONNECT_SIGNAL(wlr_device, &tablet->handlers, destroy);
}
