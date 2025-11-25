/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_INPUT_H
#define LABWC_INPUT_H

#include <wayland-server-core.h>
#include"config/gesturebind.h"

struct input {
	struct wlr_input_device *wlr_input_device;
	struct seat *seat;

	/* used by touchpad gesture */
	struct lab_gesturebind *gesture_binds[LAB_GESTURE_EVENT_COUNT][MAX_FINGERS];

	/* Set for pointer/touch devices */
	double scroll_factor;
	struct wl_listener destroy;
	struct wl_list link; /* seat.inputs */
};

void input_handlers_init(struct seat *seat);
void input_handlers_finish(struct seat *seat);

#endif /* LABWC_INPUT_H */
