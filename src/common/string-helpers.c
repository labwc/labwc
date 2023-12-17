// SPDX-License-Identifier: GPL-2.0-only
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "common/mem.h"
#include "common/string-helpers.h"

void
trim_last_field(char *buf, char delim)
{
	char *p = strrchr(buf, delim);
	if (p) {
		*p = '\0';
	}
}

static void
rtrim(char **s)
{
	size_t len = strlen(*s);
	if (!len) {
		return;
	}
	char *end = *s + len - 1;
	while (end >= *s && isspace(*end)) {
		end--;
	}
	*(end + 1) = '\0';
}

char *
string_strip(char *s)
{
	rtrim(&s);
	while (isspace(*s)) {
		s++;
	}
	return s;
}

void
string_truncate_at_pattern(char *buf, const char *pattern)
{
	char *p = strstr(buf, pattern);
	if (!p) {
		return;
	}
	*p = '\0';
}

char *
strdup_printf(const char *fmt, ...)
{
	size_t size = 0;
	char *p = NULL;
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(p, size, fmt, ap);
	va_end(ap);

	if (n < 0) {
		return NULL;
	}

	size = (size_t)n + 1;
	p = xzalloc(size);

	va_start(ap, fmt);
	n = vsnprintf(p, size, fmt, ap);
	va_end(ap);

	if (n < 0) {
		free(p);
		return NULL;
	}
	return p;
}
