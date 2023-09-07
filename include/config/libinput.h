/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_LIBINPUT_H
#define LABWC_LIBINPUT_H

#include <libinput.h>
#include <string.h>
#include <wayland-server-core.h>

enum device_type {
	DEFAULT_DEVICE,
	TOUCH_DEVICE,
	NON_TOUCH_DEVICE,
};

struct libinput_category {
	enum device_type type;
	char *name;
	struct wl_list link;
	float pointer_speed;
	int natural_scroll;
	int left_handed;
	enum libinput_config_tap_state tap;
	enum libinput_config_tap_button_map tap_button_map;
	int tap_and_drag; /* -1 or libinput_config_drag_state */
	int drag_lock; /* -1 or libinput_config_drag_lock_state */
	int accel_profile; /* -1 or libinput_config_accel_profile */
	int middle_emu; /* -1 or libinput_config_middle_emulation_state */
	int dwt; /* -1 or libinput_config_dwt_state */
};

enum device_type get_device_type(const char *s);
struct libinput_category *libinput_category_create(void);
struct libinput_category *libinput_category_get_default(void);

#endif /* LABWC_LIBINPUT_H */
