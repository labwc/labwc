/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TOUCH_CONFIG_H
#define LABWC_TOUCH_CONFIG_H

#include <stdint.h>

struct touch_config_entry {
	char *device_name;
	char *output_name;

	struct wl_list link;     /* struct rcxml.touch_configs */
};

#endif /* LABWC_TOUCH_CONFIG_H */
