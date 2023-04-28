// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include "common/match.h"

bool
match_glob(const gchar *pattern, const gchar *string)
{
	return g_pattern_match_simple(g_utf8_casefold(pattern, -1), g_utf8_casefold(string, -1));
}

