#include <stdio.h>
#include <stdlib.h>

#include "../../include/theme.h"

struct theme theme = { 0 };

int main()
{
	theme_read("../../data/themerc");
	for (int i=0; i < 4; i++)
		printf("%.2f; ", theme.window_active_title_bg_color[i]);
	printf("\n");
}
