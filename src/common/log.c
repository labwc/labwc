#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
warn(const char *err, ...)
{
	va_list params;
	fprintf(stderr, "[labwc] warn: ");
	va_start(params, err);
	vfprintf(stderr, err, params);
	va_end(params);
	fprintf(stderr, "\n");
}

