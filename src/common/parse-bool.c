// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "common/parse-bool.h"

int
parse_bool(const char *str, int default_value)
{
	if (!str) {
		goto error_not_a_boolean;
	} else if (!strcasecmp(str, "yes")) {
		return true;
	} else if (!strcasecmp(str, "true")) {
		return true;
	} else if (!strcasecmp(str, "no")) {
		return false;
	} else if (!strcasecmp(str, "false")) {
		return false;
	}
error_not_a_boolean:
	wlr_log(WLR_ERROR, "(%s) is not a boolean value", str);
	return default_value;
}

void
set_bool(const char *str, bool *variable)
{
	int ret = parse_bool(str, -1);
	if (ret < 0) {
		return;
	}
	*variable = ret;
}

void
set_bool_as_int(const char *str, int *variable)
{
	int ret = parse_bool(str, -1);
	if (ret < 0) {
		return;
	}
	*variable = ret;
}
