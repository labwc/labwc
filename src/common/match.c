// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include "common/match.h"

bool
match_glob(const gchar *pattern, const gchar *string)
{
	gchar *p = g_utf8_casefold(pattern, -1);
	gchar *s = g_utf8_casefold(string, -1);
	bool ret = g_pattern_match_simple(p, s);
	g_free(p);
	g_free(s);
	return ret;
}

