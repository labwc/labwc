/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_PAD_H
#define LABWC_TABLET_PAD_H

#include <wayland-server-core.h>
struct seat;
struct wlr_device;
struct wlr_input_device;

#define LAB_BTN_PAD 0x0
#define LAB_BTN_PAD2 0x1
#define LAB_BTN_PAD3 0x2
#define LAB_BTN_PAD4 0x3
#define LAB_BTN_PAD5 0x4
#define LAB_BTN_PAD6 0x5
#define LAB_BTN_PAD7 0x6
#define LAB_BTN_PAD8 0x7
#define LAB_BTN_PAD9 0x8

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
