/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_H
#define LABWC_TABLET_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_tablet_v2.h>

struct seat;
struct wlr_device;
struct wlr_input_device;

struct drawing_tablet {
	struct wlr_input_device *wlr_input_device;
	struct seat *seat;
	struct wlr_tablet *tablet;
	struct wlr_tablet_v2_tablet *tablet_v2;
	double x, y;
	double tilt_x, tilt_y;
	struct {
		struct wl_listener proximity;
		struct wl_listener axis;
		struct wl_listener tip;
		struct wl_listener button;
		struct wl_listener destroy;
	} handlers;
};

void tablet_init(struct seat *seat, struct wlr_input_device *wlr_input_device);

#endif /* LABWC_TABLET_H */
