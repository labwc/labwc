/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_MACROS_H
#define LABWC_MACROS_H

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

/**
 * CONNECT_SIGNAL() - Connect a signal handler function to a wl_signal.
 *
 * @param src Signal emitter (struct containing wl_signal)
 * @param dest Signal receiver (struct containing wl_listener)
 * @param name Signal name
 *
 * This assumes that the common pattern is followed where:
 *   - the wl_signal is (*src).events.<name>
 *   - the wl_listener is (*dest).<name>
 *   - the signal handler function is named handle_<name>
 */
#define CONNECT_SIGNAL(src, dest, name) \
	(dest)->name.notify = handle_##name; \
	wl_signal_add(&(src)->events.name, &(dest)->name)

/**
 * MIN() - Minimum of two values.
 *
 * @note Arguments may be evaluated twice.
 */
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/**
 * MAX() - Maximum of two values.
 *
 * @note Arguments may be evaluated twice.
 */
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#endif /* LABWC_MACROS_H */
