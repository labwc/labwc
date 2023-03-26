/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_PARSE_BOOL_H
#define __LABWC_PARSE_BOOL_H
#include <stdbool.h>

/**
 * parse_bool() - Parse boolean value of string.
 * @string: String to interpret. This check is case-insensitive.
 * @default_value: Default value to use if string is not a recognised boolean.
 *                 Use -1 to avoid setting a default value.
 *
 * Return: 0 for false; 1 for true; -1 for non-boolean
 */
int parse_bool(const char *str, int default_value);

#endif /* __LABWC_PARSE_BOOL_H */
