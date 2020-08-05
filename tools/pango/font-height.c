#include <stdio.h>
#include <stdlib.h>

#include "common/font.h"

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Usage: %s <font-description> (e.g. 'sans 10')\n", argv[0]);
		exit(1);
	}
	printf("height=%d\n", font_height(argv[1]));
}
