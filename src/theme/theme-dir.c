/*
 * Find the openbox theme directory
 *
 * Copyright Johan Malm 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

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

char *theme_dir(const char *theme_name)
{
	static char buf[4096] = { 0 };
	if (buf[0] != '\0')
		return buf;

	struct stat st;
	for (int i = 0; theme_dirs[i].path; i++) {
		char *prefix = NULL;
		struct dir d = theme_dirs[i];
		if (d.prefix) {
			prefix = getenv(d.prefix);
			if (!prefix)
				continue;
			snprintf(buf, sizeof(buf), "%s/%s/%s/openbox-3", prefix,
				 d.path, theme_name);
		} else {
			snprintf(buf, sizeof(buf), "%s/%s/openbox-3", d.path,
				 theme_name);
		}
		if (!stat(buf, &st) && S_ISDIR(st.st_mode))
			return buf;
	}
	buf[0] = '\0';
	return buf;
}
