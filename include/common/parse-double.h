/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PARSE_DOUBLE_H
#define LABWC_PARSE_DOUBLE_H
#include <assert.h>
#include <stdbool.h>

/**
 * set_double() - Parse double-precision value of string.
 * @str: String to parse
 * @val: Storage for parsed value
 *
 * Return: true if string was parsed, false if not
 *
 * NOTE: If this function returns false, the value at *val will be untouched.
 */
bool set_double(const char *str, double *val);

static inline bool
set_float(const char *str, float *val)
{
	assert(val);

	double d;
	if (set_double(str, &d)) {
		*val = d;
		return true;
	}

	return false;
}

#endif /* LABWC_PARSE_DOUBLE_H */
