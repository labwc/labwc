// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <strings.h>
#include "common/get-bool.h"

bool
get_bool(const char *s)
{
	if (!s) {
		return false;
	}
	if (!strcasecmp(s, "yes")) {
		return true;
	}
	if (!strcasecmp(s, "true")) {
		return true;
	}
	return false;
}
