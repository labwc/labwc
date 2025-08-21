// SPDX-License-Identifier: GPL-2.0-only
#include "common/parse-bool.h"
#include <strings.h>
#include <wlr/util/log.h>

enum lab_tristate
parse_tristate(const char *str)
{
	if (!str) {
		goto error_not_a_boolean;
	} else if (!strcasecmp(str, "yes")) {
		return LAB_STATE_ENABLED;
	} else if (!strcasecmp(str, "true")) {
		return LAB_STATE_ENABLED;
	} else if (!strcasecmp(str, "on")) {
		return LAB_STATE_ENABLED;
	} else if (!strcmp(str, "1")) {
		return LAB_STATE_ENABLED;
	} else if (!strcasecmp(str, "no")) {
		return LAB_STATE_DISABLED;
	} else if (!strcasecmp(str, "false")) {
		return LAB_STATE_DISABLED;
	} else if (!strcasecmp(str, "off")) {
		return LAB_STATE_DISABLED;
	} else if (!strcmp(str, "0")) {
		return LAB_STATE_DISABLED;
	}
error_not_a_boolean:
	wlr_log(WLR_ERROR, "(%s) is not a boolean value", str);
	return LAB_STATE_UNSPECIFIED;
}

int
parse_bool(const char *str, int default_value)
{
	enum lab_tristate val = parse_tristate(str);
	if (val == LAB_STATE_UNSPECIFIED) {
		return default_value;
	}
	return (val == LAB_STATE_ENABLED) ? 1 : 0;
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
