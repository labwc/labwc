// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <strings.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "config/rcxml.h"

static struct touch_config_entry *
find_default_config(void)
{
	struct touch_config_entry *entry;
	wl_list_for_each(entry, &rc.touch_configs, link) {
		if (!entry->device_name) {
			wlr_log(WLR_INFO, "found default touch configuration");
			return entry;
		}
	}
	return NULL;
}

struct touch_config_entry *
touch_find_config_for_device(char *device_name)
{
	wlr_log(WLR_INFO, "find touch configuration for %s\n", device_name);
	struct touch_config_entry *entry;
	wl_list_for_each(entry, &rc.touch_configs, link) {
		if (entry->device_name && !strcasecmp(entry->device_name, device_name)) {
			wlr_log(WLR_INFO, "found touch configuration for %s\n", device_name);
			return entry;
		}
	}
	return find_default_config();
}
