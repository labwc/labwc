// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <unistd.h>
#include "button/common.h"
#include "common/dir.h"
#include "config/rcxml.h"
#include "labwc.h"

void
button_filename(const char *name, char *buf, size_t len)
{
	struct wl_list paths;
	paths_theme_create(&paths, rc.theme_name, name);

	/*
	 * You can't really merge buttons, so let's just iterate forwards
	 * and stop on the first hit
	 */
	struct path *path;
	wl_list_for_each(path, &paths, link) {
		if (access(path->string, R_OK) == 0) {
			snprintf(buf, len, "%s", path->string);
			break;
		}
	}
	paths_destroy(&paths);
}
