// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "input/cursor.h"
#include "input/tablet-pad.h"
#include "input/tablet.h"
#include "labwc.h"

void
tablet_pad_attach_tablet(struct seat *seat)
{
	/* reset all tablet - pad links */
	struct drawing_tablet_pad *pad;
	wl_list_for_each(pad, &seat->tablet_pads, link) {
		pad->tablet = NULL;
	}

	/* loop over all tablets and all pads and link by device group */
	struct drawing_tablet *tablet;
	wl_list_for_each(tablet, &seat->tablets, link) {
		if (!wlr_input_device_is_libinput(tablet->wlr_input_device)) {
			/*
			 * Prevent iterating over non-libinput devices. This might
			 * be the case when a tablet is exposed by the Wayland
			 * protocol backend when running labwc as a nested compositor.
			 */
			continue;
		}

		struct libinput_device *tablet_device =
			wlr_libinput_get_device_handle(tablet->wlr_input_device);
		struct libinput_device_group *tablet_group =
			libinput_device_get_device_group(tablet_device);

		wl_list_for_each(pad, &seat->tablet_pads, link) {
			if (!wlr_input_device_is_libinput(pad->wlr_input_device)) {
				continue;
			}

			struct libinput_device *pad_device =
				wlr_libinput_get_device_handle(pad->wlr_input_device);
			struct libinput_device_group *pad_group =
				libinput_device_get_device_group(pad_device);

			if (tablet_group == pad_group) {
				wlr_log(WLR_DEBUG, "attach tablet to pad based on device group");
				pad->tablet = tablet;
			}
		}
	}
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_pad *pad =
		wl_container_of(listener, pad, handlers.current_surface_destroy);

	pad->current_surface = NULL;
	wl_list_remove(&pad->handlers.current_surface_destroy.link);
}

void
tablet_pad_enter_surface(struct seat *seat, struct wlr_surface *surface)
{
	if (!surface) {
		return;
	}

	struct drawing_tablet_pad *pad;
	wl_list_for_each(pad, &seat->tablet_pads, link) {
		/* pad needs a linked tablet and both need tablet-v2 support */
		if (pad->tablet && pad->pad_v2 && pad->tablet->tablet_v2) {
			if (pad->current_surface) {
				wlr_tablet_v2_tablet_pad_notify_leave(pad->pad_v2,
					pad->current_surface);

				/* remove previous surface destroy handler */
				wl_list_remove(&pad->handlers.current_surface_destroy.link);
			}

			pad->current_surface = surface;
			wlr_tablet_v2_tablet_pad_notify_enter(pad->pad_v2,
				pad->tablet->tablet_v2, surface);

			/* signal surface destroy handler */
			wl_signal_add(&pad->current_surface->events.destroy,
				&pad->handlers.current_surface_destroy);
			pad->handlers.current_surface_destroy.notify =
				handle_surface_destroy;
		}
	}
}

static void
handle_button(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_pad *pad =
		wl_container_of(listener, pad, handlers.button);
	struct wlr_tablet_pad_button_event *ev = data;

	if (!rc.tablet.force_mouse_emulation
			&& pad->pad_v2 && pad->current_surface) {
		wlr_tablet_v2_tablet_pad_notify_button(pad->pad_v2, ev->button,
			ev->time_msec,
			ev->state == WLR_BUTTON_PRESSED
				? ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED
				: ZWP_TABLET_PAD_V2_BUTTON_STATE_RELEASED);
	} else {
		uint32_t button = tablet_get_mapped_button(ev->button);
		if (button) {
			cursor_emulate_button(pad->seat, button,
				ev->state == WLR_BUTTON_PRESSED
					? WL_POINTER_BUTTON_STATE_PRESSED
					: WL_POINTER_BUTTON_STATE_RELEASED,
				ev->time_msec);
		}
	}
}

static void
handle_ring(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_pad *pad =
		wl_container_of(listener, pad, handlers.ring);
	struct wlr_tablet_pad_ring_event *ev = data;

	if (!rc.tablet.force_mouse_emulation
			&& pad->pad_v2 && pad->current_surface) {
		wlr_tablet_v2_tablet_pad_notify_ring(pad->pad_v2,
			ev->ring, ev->position,
			ev->source == WLR_TABLET_PAD_RING_SOURCE_FINGER,
			ev->time_msec);
	}
}

static void
handle_strip(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_pad *pad =
		wl_container_of(listener, pad, handlers.strip);
	struct wlr_tablet_pad_strip_event *ev = data;

	if (!rc.tablet.force_mouse_emulation
			&& pad->pad_v2 && pad->current_surface) {
		wlr_tablet_v2_tablet_pad_notify_strip(pad->pad_v2,
			ev->strip, ev->position,
			ev->source == WLR_TABLET_PAD_STRIP_SOURCE_FINGER,
			ev->time_msec);
	}
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet_pad *pad =
		wl_container_of(listener, pad, handlers.destroy);

	if (pad->current_surface) {
		wl_list_remove(&pad->handlers.current_surface_destroy.link);
	}
	wl_list_remove(&pad->link);
	wl_list_remove(&pad->handlers.button.link);
	wl_list_remove(&pad->handlers.ring.link);
	wl_list_remove(&pad->handlers.strip.link);
	wl_list_remove(&pad->handlers.destroy.link);
	free(pad);
}

void
tablet_pad_init(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_DEBUG, "setting up tablet pad");
	struct drawing_tablet_pad *pad = znew(*pad);
	pad->seat = seat;
	pad->wlr_input_device = wlr_device;
	pad->pad = wlr_tablet_pad_from_input_device(wlr_device);
	if (seat->server->tablet_manager) {
		pad->pad_v2 = wlr_tablet_pad_create(
			seat->server->tablet_manager, seat->seat, wlr_device);
	}
	pad->pad->data = pad;
	CONNECT_SIGNAL(pad->pad, &pad->handlers, button);
	CONNECT_SIGNAL(pad->pad, &pad->handlers, ring);
	CONNECT_SIGNAL(pad->pad, &pad->handlers, strip);
	CONNECT_SIGNAL(wlr_device, &pad->handlers, destroy);
	wl_list_insert(&seat->tablet_pads, &pad->link);
	tablet_pad_attach_tablet(pad->seat);
}
