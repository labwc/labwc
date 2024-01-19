/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_STRING_HELPERS_H
#define LABWC_STRING_HELPERS_H
#include <stdbool.h>

/**
 * string_null_or_empty() - Check if string is NULL or empty
 * @s: string to check
 */
bool string_null_or_empty(const char *s);

/**
 * trim_last_field() - Trim last field of string splitting on provided delim
 * @buf: string to trim
 * @delim: delimitator
 *
 * Example: With delim='_' and buf="foo_bar_baz" the return value is "foo_bar"
 */
void trim_last_field(char *buf, char delim);

/**
 * string_strip - strip white space left and right
 * Note: this function does a left skip, so the returning pointer cannot be
 * used to free any allocated memory
 */
char *string_strip(char *s);

/**
 * string_truncate_at_pattern - remove pattern and everything after it
 * @buf: pointer to buffer
 * @pattern: string to remove
 */
void string_truncate_at_pattern(char *buf, const char *pattern);

/**
 * strdup_printf - allocate and write to buffer in printf format
 * @fmt: printf-style format.
 *
 * Similar to the standard C sprintf() function but safer as it calculates the
 * maximum space required and allocates memory to hold the output.
 * The user must free the returned string.
 * Returns NULL on error.
 */
char *strdup_printf(const char *fmt, ...);

#endif /* LABWC_STRING_HELPERS_H */
