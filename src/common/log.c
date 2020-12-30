#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
info(const char *msg, ...)
{
	va_list params;
	fprintf(stderr, "[labwc] info: ");
	va_start(params, msg);
	vfprintf(stderr, msg, params);
	va_end(params);
	fprintf(stderr, "\n");
}

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

void
die(const char *err, ...)
{
	va_list params;
	fprintf(stderr, "[labwc] fatal: ");
	va_start(params, err);
	vfprintf(stderr, err, params);
	va_end(params);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}
