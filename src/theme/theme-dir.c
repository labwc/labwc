/*
 * Find the openbox theme directory
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <glib.h>

struct dir {
	const char *prefix;
	const char *path;
};

/* clang-format off */
static struct dir theme_dirs[] = {
	{ "XDG_DATA_HOME", "themes" },
	{ "HOME", ".local/share/themes" },
	{ "HOME", ".themes" },
	{ "XDG_DATA_DIRS", "themes" },
	{ NULL, "/usr/share/themes" },
	{ NULL, "/usr/local/share/themes" },
	{ NULL, "opt/share/themes" },
	{ NULL, NULL }
};
/* clang-format on */

static bool isdir(const char *path)
{
	struct stat st;
	return (!stat(path, &st) && S_ISDIR(st.st_mode));
}

char *theme_dir(const char *theme_name)
{
	static char buf[4096] = { 0 };
	if (buf[0] != '\0')
		return buf;

	for (int i = 0; theme_dirs[i].path; i++) {
		struct dir d = theme_dirs[i];
		if (!d.prefix) {
			snprintf(buf, sizeof(buf), "%s/%s/openbox-3", d.path,
				 theme_name);
			if (isdir(buf))
				return buf;
		} else {
			char *prefix = getenv(d.prefix);
			if (!prefix)
				continue;
			gchar **prefixes = g_strsplit(prefix, ":", -1);
			for (gchar **p = prefixes; *p; p++) {
				snprintf(buf, sizeof(buf),
					 "%s/%s/%s/openbox-3", *p, d.path,
					 theme_name);
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
