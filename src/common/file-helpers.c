// SPDX-License-Identifier: GPL-2.0-only
#include "common/file-helpers.h"
#include <sys/stat.h>

bool
file_exists(const char *filename)
{
	struct stat st;
	return (!stat(filename, &st));
}
