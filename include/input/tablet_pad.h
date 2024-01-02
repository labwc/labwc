/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_PAD_H
#define LABWC_TABLET_PAD_H

#include <wayland-server-core.h>
struct seat;
struct wlr_device;
struct wlr_input_device;

struct drawing_tablet_pad {
	struct seat *seat;
	struct wlr_tablet_pad *tablet;
	struct {
		struct wl_listener button;
		struct wl_listener destroy;
	} handlers;
};

void tablet_pad_init(struct seat *seat, struct wlr_input_device *wlr_input_device);

#endif /* LABWC_TABLET_PAD_H */
