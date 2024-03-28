// SPDX-License-Identifier: GPL-2.0-only
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "common/parse-double.h"

struct dec_separator {
	int index;
	bool multiple;
};

struct converted_double {
	double value;
	bool valid;
};

static struct dec_separator
find_dec_separator(const char *str)
{
	struct dec_separator loc = {
		.index = -1,
		.multiple = false
	};

	for (int i = 0; *str; i++, str++) {
		switch (*str) {
		case ',':
		case '.':
			if (loc.index >= 0) {
				loc.multiple = true;
				return loc;
			} else {
				loc.index = i;
			}
			break;
		}
	}

	return loc;
}

static struct converted_double
convert_double(const char *str)
{
	struct converted_double result = {
		.value = 0,
		.valid = true,
	};

	char *eptr = NULL;

	errno = 0;
	result.value = strtod(str, &eptr);

	if (errno) {
		wlr_log(WLR_ERROR, "value '%s' is out of range", str);
		result.valid = false;
	}

	if (*eptr) {
		wlr_log(WLR_ERROR, "value '%s' contains trailing garbage", str);
		result.valid = false;
	}

	return result;
}

bool
set_double(const char *str, double *val)
{
	assert(str);
	assert(val);

	struct dec_separator dloc = find_dec_separator(str);
	if (dloc.multiple) {
		wlr_log(WLR_ERROR,
			"value '%s' contains multiple decimal markers", str);
		return false;
	}

	char *lstr = NULL;

	if (dloc.index >= 0) {
		lstr = xstrdup(str);
		struct lconv *lc = localeconv();
		lstr[dloc.index] = *lc->decimal_point;
		str = lstr;
	}

	struct converted_double conv = convert_double(str);
	if (conv.valid) {
		*val = conv.value;
	}

	free(lstr);
	return conv.valid;
}
