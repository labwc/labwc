/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_LIBINPUT_H
#define LABWC_LIBINPUT_H

#include <libinput.h>
#include <libxml/tree.h>
#include <string.h>
#include <wayland-server-core.h>

enum lab_libinput_device_type {
	LAB_LIBINPUT_DEVICE_NONE = 0,
	LAB_LIBINPUT_DEVICE_DEFAULT,
	LAB_LIBINPUT_DEVICE_TOUCH,
	LAB_LIBINPUT_DEVICE_TOUCHPAD,
	LAB_LIBINPUT_DEVICE_NON_TOUCH,
};

#define LAB_ACCEL_CURVE_MAX_POINTS 32

struct lab_accel_curve {
	double step;
	double points[LAB_ACCEL_CURVE_MAX_POINTS];
	size_t nr_points;
};

struct libinput_category {
	enum lab_libinput_device_type type;
	char *name;
	struct wl_list link;
	float pointer_speed;
	int natural_scroll;
	int left_handed;
	enum libinput_config_tap_state tap;
	enum libinput_config_tap_button_map tap_button_map;
	int tap_and_drag;               /* -1 or libinput_config_drag_state */
	int drag_lock;                  /* -1 or libinput_config_drag_lock_state */
	int three_finger_drag;          /* -1 or libinput_config_3fg_drag_state */
	int accel_profile;              /* -1 or libinput_config_accel_profile */
	int middle_emu;                 /* -1 or libinput_config_middle_emulation_state */
	int dwt;                        /* -1 or libinput_config_dwt_state */
	int click_method;               /* -1 or libinput_config_click_method */
	int scroll_method;              /* -1 or libinput_config_scroll_method */
	int scroll_button;              /* -1 or a button from linux/input_event_codes.h */
	int send_events_mode;           /* -1 or libinput_config_send_events_mode */
	bool have_calibration_matrix;
	double scroll_factor;
	float calibration_matrix[6];
	struct lab_accel_curve accel_curves[LIBINPUT_ACCEL_TYPE_SCROLL + 1];
	bool accel_curve_invalid;
};

enum lab_libinput_device_type get_device_type(const char *s);
const char *libinput_device_type_name(enum lab_libinput_device_type type);
struct libinput_category *libinput_category_create(void);
struct libinput_category *libinput_category_get_default(void);

/* Parse after XML attribute expansion; leave curve unchanged on failure. */
bool libinput_curve_parse(xmlNode *node, struct lab_accel_curve *curve);
/* Returns a fresh config owned by the caller, or NULL if any curve is invalid. */
struct libinput_config_accel *libinput_custom_accel_create(
	const struct libinput_category *category);

#endif /* LABWC_LIBINPUT_H */
