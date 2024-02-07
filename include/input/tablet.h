/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_H
#define LABWC_TABLET_H

#include <wayland-server-core.h>

struct seat;
struct wlr_device;
struct wlr_input_device;

struct drawing_tablet {
	struct seat *seat;
	struct wlr_tablet *tablet;
	double x, y;
	struct {
		struct wl_listener axis;
		struct wl_listener tip;
		struct wl_listener button;
		struct wl_listener destroy;
		// no interest in proximity events
	} handlers;
};

void tablet_init(struct seat *seat, struct wlr_input_device *wlr_input_device);

#endif /* LABWC_TABLET_H */
