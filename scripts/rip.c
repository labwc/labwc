// SPDX-License-Identifier: GPL-2.0-only
/*
 * Helper to extract references from NEWS.md and print them as github URLs
 * Usage: gcc -o rip scripts/rip.c && ./rip < NEWS.md
 *
 * Copyright (C) Johan Malm 2025
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int *refs;
static int nr_refs, alloc_refs;

static void
add_ref(int num)
{
	if (nr_refs == alloc_refs) {
		alloc_refs = (alloc_refs + 16) * 2;
		refs = realloc(refs, alloc_refs * sizeof(int));
	}
	int *ref = refs + nr_refs;
	nr_refs++;
	*ref = num;
}

static bool
is_unique(int num)
{
	for (int i = 0; i < nr_refs; ++i) {
		if (refs[i] == num) {
			return false;
		}
	}
	return true;
}

void process_line(const char *line)
{
	const char *p = line;
	while ((p = strchr(p, '['))) {
		int n, num = 0;
		n = sscanf(p, "[#%d]", &num);
		if (n && is_unique(num)) {
			add_ref(num);
		}
		++p;
	}
}

static int
compare_ints(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

int main(int argc, char **argv)
{
	char *line = NULL; size_t len = 0;
	while (getline(&line, &len, stdin) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		/* Do not process the references at the bottom of NEWS.md */
		if (!strncmp(line, "[0.1.0-commits]", 15)) {
			break;
		}
		process_line(line);
	}
	free(line);
	qsort(refs, nr_refs, sizeof(int), compare_ints);
	for (int i = 0; i < nr_refs; ++i) {
		int r = refs[i];
		/* GH groks the .../pull/%d format for both issues and PRs */
		printf("[#%d]: https://github.com/labwc/labwc/pull/%d\n", r, r);
	}
	free(refs);
}
