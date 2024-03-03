// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "common/mem.h"
#include "common/string-helpers.h"

bool
string_null_or_empty(const char *s)
{
	return !s || !*s;
}

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

char *
str_join(const char * const parts[],
		const char *restrict fmt, const char *restrict sep)
{
	assert(parts);

	if (!fmt) {
		fmt = "%s";
	}

	if (!sep) {
		sep = " ";
	}

	size_t size = 0;
	size_t n_parts = 0;

	size_t sep_len = strlen(sep);

	/* Count the length of each formatted string */
	for (const char *const *s = parts; *s; ++s) {
		int n = snprintf(NULL, 0, fmt, *s);
		if (n < 0) {
			return NULL;
		}
		size += (size_t)n;
		++n_parts;
	}

	if (n_parts < 1) {
		return NULL;
	}

	/* Need (n_parts - 1) separators, plus one NULL terminator */
	size += (n_parts - 1) * sep_len + 1;

	/* Concatenate the strings and separators */
	char *buf = xzalloc(size);
	char *p = buf;
	for (const char *const *s = parts; *s; ++s) {
		int n = 0;

		if (p != buf) {
			n = snprintf(p, size, "%s", sep);
			if (n < 0 || (size_t)n >= size) {
				p = NULL;
				break;
			}
			size -= (size_t)n;
			p += (size_t)n;
		}

		n = snprintf(p, size, fmt, *s);
		if (n < 0 || (size_t)n >= size) {
			p = NULL;
			break;
		}
		size -= (size_t)n;
		p += (size_t)n;
	}

	if (!p) {
		free(buf);
		return NULL;
	}

	return buf;
}

