/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_GET_BOOL_H
#define __LABWC_GET_BOOL_H
#include <stdbool.h>

/**
 * get_bool - interpret string and return boolean
 * @s: string to interpret
 *
 * Note: This merely performs a case-insensitive check for 'yes' and 'true'.
 * Returns false by default.
 */
bool get_bool(const char *s);

#endif /* __LABWC_GET_BOOL_H */
