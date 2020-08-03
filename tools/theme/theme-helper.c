#include <stdio.h>
#include <stdlib.h>

#include "theme/theme.h"

struct theme theme = { 0 };

static void usage(const char *application)
{
	printf("Usage: %s <theme-name>\n", application);
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		usage(argv[0]);
	theme_read(argv[1]);

	printf("window_active_title_bg_color = ");
	for (int i=0; i < 4; i++)
		printf("%.2f; ", theme.window_active_title_bg_color[i]);
	printf("\n");
}
