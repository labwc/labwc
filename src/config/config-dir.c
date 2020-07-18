/*
 * Find the labwc configuration directory
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <glib.h>

#include "config/config-dir.h"

struct dir {
	const char *prefix;
	const char *path;
};

/* clang-format off */
static struct dir config_dirs[] = {
	{ "XDG_CONFIG_HOME", "labwc" },
	{ "HOME", ".config/labwc" },
	{ "XDG_CONFIG_DIRS", "labwc" },
	{ NULL, "/etc/xdg/labwc" },
	{ "XDG_CONFIG_HOME", "openbox" },
	{ "HOME", ".config/openbox" },
	{ "XDG_CONFIG_DIRS", "openbox" },
	{ NULL, "/etc/xdg/openbox" },
	{ NULL, NULL }
};
/* clang-format on */

static bool isdir(const char *path)
{
	struct stat st;
	return (!stat(path, &st) && S_ISDIR(st.st_mode));
}

char *config_dir(void)
{
	static char buf[4096] = { 0 };
	if (buf[0] != '\0')
		return buf;

	for (int i = 0; config_dirs[i].path; i++) {
		struct dir d = config_dirs[i];
		if (!d.prefix) {
			/* handle /etc/xdg... */
			snprintf(buf, sizeof(buf), "%s", d.path);
			if (isdir(buf))
				return buf;
		} else {
			/* handle $HOME/.config/... and $XDG_* */
			char *prefix = getenv(d.prefix);
			if (!prefix)
				continue;
			gchar **prefixes = g_strsplit(prefix, ":", -1);
			for (gchar **p = prefixes; *p; p++) {
				snprintf(buf, sizeof(buf), "%s/%s", *p, d.path);
				if (isdir(buf))
					return buf;
			}
		}
	}
	/* no config directory was found */
	buf[0] = '.';
	buf[1] = '\0';
	return buf;
}
