// SPDX-License-Identifier: GPL-2.0-only

#include "common/match.h"
#include <fnmatch.h>

bool
match_glob(const char *pattern, const char *string)
{
	return fnmatch(pattern, string, FNM_CASEFOLD) == 0;
}
