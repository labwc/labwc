// SPDX-License-Identifier: GPL-2.0-only

#include <fnmatch.h>
#include "common/match.h"

bool
match_glob(const char *pattern, const char *string)
{
	return fnmatch(pattern, string, FNM_CASEFOLD) == 0;
}
