/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PARSE_BOOL_H
#define LABWC_PARSE_BOOL_H
#include <stdbool.h>
#include "common/three-state.h"

/**
 * parse_three_state() - Parse boolean value of string as a three-state enum.
 * @string: String to interpret. This check is case-insensitive.
 *
 * Return: LAB_STATE_DISABLED for false; LAB_STATE_ENABLED for true;
 * LAB_STATE_UNSPECIFIED for non-boolean
 */
enum three_state parse_three_state(const char *str);

/**
 * parse_bool() - Parse boolean value of string.
 * @string: String to interpret. This check is case-insensitive.
 * @default_value: Default value to use if string is not a recognised boolean.
 *                 Use -1 to avoid setting a default value.
 *
 * Return: 0 for false; 1 for true; -1 for non-boolean
 */
int parse_bool(const char *str, int default_value);

/**
 * set_bool() - Parse boolean text and set variable iff text is valid boolean
 * @string: Boolean text to interpret.
 * @variable: Variable to set.
 */
void set_bool(const char *str, bool *variable);
void set_bool_as_int(const char *str, int *variable);

#endif /* LABWC_PARSE_BOOL_H */
