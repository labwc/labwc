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
