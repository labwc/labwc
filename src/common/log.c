#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LABWC_COLOR_YELLOW "\033[0;33m"
#define LABWC_COLOR_RED "\033[0;31m"
#define LABWC_COLOR_RESET "\033[0m"

void
info(const char *msg, ...)
{
	va_list params;
	fprintf(stderr, LABWC_COLOR_YELLOW);
	fprintf(stderr, "[labwc] ");
	va_start(params, msg);
	vfprintf(stderr, msg, params);
	va_end(params);
	fprintf(stderr, LABWC_COLOR_RESET);
	fprintf(stderr, "\n");
}

void
warn(const char *err, ...)
{
	va_list params;
	fprintf(stderr, LABWC_COLOR_RED);
	fprintf(stderr, "[labwc] ");
	va_start(params, err);
	vfprintf(stderr, err, params);
	va_end(params);
	fprintf(stderr, LABWC_COLOR_RESET);
	fprintf(stderr, "\n");
}
