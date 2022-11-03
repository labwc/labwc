/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_MEM_H
#define __LABWC_MEM_H

#include <stdlib.h>

/*
 * As defined in busybox, weston, etc.
 * Allocates zero-filled memory; calls exit() on error.
 * Returns NULL only if (size == 0).
 */
void *xzalloc(size_t size);

/*
 * Type-safe macros in the style of C++ new/new[].
 * Both allocate zero-filled memory for object(s) the same size as
 * <expr>, which may be either a type name or value expression.
 *
 * znew() allocates space for one object.
 * znew_n() allocates space for an array of <n> objects.
 *
 * Examples:
 *   struct wlr_box *box = znew(*box);
 *   char *buf = znew_n(char, 80);
 */
#define znew(expr)       ((__typeof__(expr) *)xzalloc(sizeof(expr)))
#define znew_n(expr, n)  ((__typeof__(expr) *)xzalloc((n) * sizeof(expr)))

/*
 * As defined in FreeBSD.
 * Like realloc(), but calls exit() on error.
 * Returns NULL only if (size == 0).
 * Does NOT zero-fill memory.
 */
void *xrealloc(void *ptr, size_t size);

/* malloc() is a specific case of realloc() */
#define xmalloc(size) xrealloc(NULL, (size))

/*
 * As defined in FreeBSD.
 * Allocates a copy of <str>; calls exit() on error.
 * Requires (str != NULL) and never returns NULL.
 */
char *xstrdup(const char *str);

/*
 * Frees memory pointed to by <ptr> and sets <ptr> to NULL.
 * Does nothing if <ptr> is already NULL.
 */
#define zfree(ptr) do { \
	free(ptr); (ptr) = NULL; \
} while (0)

#endif /* __LABWC_MEM_H */
