// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include "button/common.h"
#include "common/dir.h"
#include "config/rcxml.h"

void
button_filename(const char *name, char *buf, size_t len)
{
	snprintf(buf, len, "%s/%s", theme_dir(rc.theme_name), name);
}
