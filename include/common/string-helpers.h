/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_STRING_HELPERS_H
#define LABWC_STRING_HELPERS_H

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
