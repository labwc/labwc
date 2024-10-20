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
	enum motion motion_mode;
	double x, y, dx, dy;
	double distance;
	double pressure;
	double tilt_x, tilt_y;
	double rotation;
	double slider;
	double wheel_delta;
	struct {
		struct wl_listener destroy;
	} handlers;
	struct wl_list link; /* seat.tablets */
};

void tablet_init(struct seat *seat);
void tablet_finish(struct seat *seat);
void tablet_create(struct seat *seat, struct wlr_input_device *wlr_input_device);

#endif /* LABWC_TABLET_H */
