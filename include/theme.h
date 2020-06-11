#ifndef THEME_H
#define THEME_H

#include <stdio.h>

struct theme {
	float window_active_title_bg_color[4];
};

extern struct theme theme;

void theme_read(const char *filename);

#endif /* THEME_H */
