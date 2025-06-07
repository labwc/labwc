// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <strings.h>
#include "config.h"
#include "common/mem.h"
#include "common/list.h"
#include "common/string-helpers.h"
#include "config/libinput.h"
#include "config/rcxml.h"

static void
libinput_category_init(struct libinput_category *l)
{
	l->type = LAB_LIBINPUT_DEVICE_DEFAULT;
	l->name = NULL;
	l->pointer_speed = -2;
	l->natural_scroll = -1;
	l->left_handed = -1;
	l->tap = LIBINPUT_CONFIG_TAP_ENABLED;
	l->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
	l->tap_and_drag = -1;
#if HAVE_LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY
	l->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY;
#else
	l->drag_lock = -1;
#endif
	l->three_finger_drag = -1;
	l->accel_profile = -1;
	l->middle_emu = -1;
	l->dwt = -1;
	l->click_method = -1;
	l->scroll_method = -1;
	l->send_events_mode = -1;
	l->have_calibration_matrix = false;
	l->scroll_factor = 1.0;
}

enum lab_libinput_device_type
get_device_type(const char *s)
{
	if (string_null_or_empty(s)) {
		return LAB_LIBINPUT_DEVICE_NONE;
	}
	if (!strcasecmp(s, "default")) {
		return LAB_LIBINPUT_DEVICE_DEFAULT;
	}
	if (!strcasecmp(s, "touch")) {
		return LAB_LIBINPUT_DEVICE_TOUCH;
	}
	if (!strcasecmp(s, "touchpad")) {
		return LAB_LIBINPUT_DEVICE_TOUCHPAD;
	}
	if (!strcasecmp(s, "non-touch")) {
		return LAB_LIBINPUT_DEVICE_NON_TOUCH;
	}
	return LAB_LIBINPUT_DEVICE_NONE;
}

struct libinput_category *
libinput_category_create(void)
{
	struct libinput_category *l = znew(*l);
	libinput_category_init(l);
	wl_list_append(&rc.libinput_categories, &l->link);
	return l;
}

/* After rcxml_read(), a default category always exists. */
struct libinput_category *
libinput_category_get_default(void)
{
	struct libinput_category *l;
	/*
	 * Iterate in reverse to get the last one added in case multiple
	 * 'default' profiles were created.
	 */
	wl_list_for_each_reverse(l, &rc.libinput_categories, link) {
		if (l->type == LAB_LIBINPUT_DEVICE_DEFAULT) {
			return l;
		}
	}
	return NULL;
}
