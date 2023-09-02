// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <strings.h>

#include "common/mem.h"
#include "config/libinput.h"
#include "config/rcxml.h"

static void
libinput_category_init(struct libinput_category *l)
{
	l->type = DEFAULT_DEVICE;
	l->name = NULL;
	l->pointer_speed = -2;
	l->natural_scroll = -1;
	l->left_handed = -1;
	l->tap = LIBINPUT_CONFIG_TAP_ENABLED;
	l->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
	l->tap_and_drag = -1;
	l->drag_lock = -1;
	l->accel_profile = -1;
	l->middle_emu = -1;
	l->dwt = -1;
}

enum device_type
get_device_type(const char *s)
{
	if (!s) {
		return DEFAULT_DEVICE;
	}
	if (!strcasecmp(s, "touch")) {
		return TOUCH_DEVICE;
	}
	if (!strcasecmp(s, "non-touch")) {
		return NON_TOUCH_DEVICE;
	}
	return DEFAULT_DEVICE;
}

struct libinput_category *
libinput_category_create(void)
{
	struct libinput_category *l = znew(*l);
	libinput_category_init(l);
	wl_list_insert(&rc.libinput_categories, &l->link);
	return l;
}

/*
 * The default category is the first one with type == DEFAULT_DEVICE and
 * no name. After rcxml_read(), a default category always exists.
 */
struct libinput_category *
libinput_category_get_default(void)
{
	struct libinput_category *l;
	wl_list_for_each(l, &rc.libinput_categories, link) {
		if (l->type == DEFAULT_DEVICE && !l->name) {
			return l;
		}
	}
	return NULL;
}
