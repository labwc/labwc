// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <strings.h>
#include <wlr/util/log.h>
#include "config/tablet-tool.h"

enum motion
tablet_parse_motion(const char *name)
{
	if (!strcasecmp(name, "Absolute")) {
		return LAB_TABLET_MOTION_ABSOLUTE;
	} else if (!strcasecmp(name, "Relative")) {
		return LAB_TABLET_MOTION_RELATIVE;
	}
	wlr_log(WLR_ERROR, "Invalid value for tablet motion: %s", name);
	return LAB_TABLET_MOTION_ABSOLUTE;
}
