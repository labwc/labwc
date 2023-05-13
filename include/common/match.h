/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_MATCH_H
#define LABWC_MATCH_H
#include <glib.h>

/**
 * match_glob() - Pattern match using '*' wildcards and '?' jokers.
 * @pattern: Pattern to match against.
 * @string: String to search.
 * Note: Comparison case-insensitive.
 */
bool match_glob(const gchar *pattern, const gchar *string);

#endif /* LABWC_MATCH_H */
