// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "common/mem.h"

static void
die_if_null(void *ptr)
{
	if (!ptr) {
		perror("Failed to allocate memory");
		exit(EXIT_FAILURE);
	}
}

void *
xzalloc(size_t size)
{
	if (!size) {
		return NULL;
	}
	void *ptr = calloc(1, size);
	die_if_null(ptr);
	return ptr;
}

void *
xrealloc(void *ptr, size_t size)
{
	if (!size) {
		free(ptr);
		return NULL;
	}
	ptr = realloc(ptr, size);
	die_if_null(ptr);
	return ptr;
}

char *
xstrdup(const char *str)
{
	assert(str);
	char *copy = strdup(str);
	die_if_null(copy);
	return copy;
}
