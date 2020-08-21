#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hexval(int c)
{
	int ret = -1;
	switch (c) {
	case '0'...'9':
		ret = c - '0';
		break;
	case 'a'...'f':
		ret = c - 'a' + 10;
		break;
	case 'A'...'F':
		ret = c - 'A' + 10;
		break;
	}
	return ret;
}

int hex2dec(const char *hexstring)
{
	int value = 0, pos = 0, hex;
	while ((hex = hexval(hexstring[pos++])) != -1)
		value = (value << 4) + hex;
	return value;
}

void usage(const char *command)
{
	printf("Usage: %s <rrggbb> <rrggbb>\n", command);
	exit(1);
}
int main(int argc, char **argv)
{
	double col[6] = { 0 };

	if (argc < 3)
		usage(argv[0]);

	for (int j = 1; j < argc; j++) {
		int len = strlen(argv[j]);
		for (int i = 0; i < len / 2; i++) {
			char buf[3] = { 0 };
			buf[0] = argv[j][i * 2];
			buf[1] = argv[j][i * 2 + 1];
			col[(j - 1) * 3 + i] = hex2dec(buf) / 255.0;
		}
	}
	printf("[%s] { %.2f, %.2f, %.2f }\n", argv[1], col[0], col[1], col[2]);
	printf("[%s] { %.2f, %.2f, %.2f }\n", argv[2], col[3], col[4], col[5]);
	printf("[ mean ] { %.2f, %.2f, %.2f }\n",
	       (col[0] + col[3]) / 2.0,
	       (col[1] + col[4]) / 2.0,
	       (col[2] + col[5]) / 2.0);
	printf("[ mean ] #%x%x%x\n",
	       (int)((col[0] + col[3]) / 2.0 * 255),
	       (int)((col[1] + col[4]) / 2.0 * 255),
	       (int)((col[2] + col[5]) / 2.0 * 255));
}

