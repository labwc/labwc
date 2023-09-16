/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ARRAY_SIZE_H
#define LABWC_ARRAY_SIZE_H

/**
 * ARRAY_SIZE() - Get the number of elements in array.
 * @arr: array to be sized
 *
 * This does not work on pointers.
 *
 * Recent versions of GCC and clang support -Werror=sizeof-pointer-div
 * and thus avoids using constructs such as:
 *
 * #define same_type(a, b) (__builtin_types_compatible_p(typeof(a), typeof(b)) == 1)
 * #define ARRAY_SIZE(a) ({ static_assert(!same_type(a, &(a)[0])); sizeof(a) / sizeof(a[0]); })
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif /* LABWC_ARRAY_SIZE_H */
