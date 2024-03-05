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

/**
 * str_join - format and join an array of strings with a separator
 * @parts: NULL-terminated array of string parts to be joined
 * @fmt: printf-style format string applied to each part
 * @sep: separator inserted between parts when joining
 *
 * A new string is allocated to hold the joined result. The user must free the
 * returned string. Returns NULL on error.
 *
 * Each part of the array is converted via the equivalent of sprintf(output,
 * fmt, part), so fmt should include a single "%s" format specification. If fmt
 * is NULL, a default "%s" will be used to copy each part verbatim.
 *
 * The separator is arbitrary. When the separator is NULL, a single space will
 * be used.
 */
char *str_join(const char * const parts[],
	const char *restrict fmt, const char *restrict sep);

#endif /* LABWC_STRING_HELPERS_H */
