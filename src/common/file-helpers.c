// SPDX-License-Identifier: GPL-2.0-only
#include <sys/stat.h>
#include "common/file-helpers.h"

bool
file_exists(const char *filename)
{
	struct stat st;
	return (!stat(filename, &st));
}
