// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L

#include <sys/resource.h>
#include <wlr/util/log.h>

#include "common/fd_util.h"

static struct rlimit original_nofile_rlimit = {0};

void
increase_nofile_limit(void)
{
	if (getrlimit(RLIMIT_NOFILE, &original_nofile_rlimit) != 0) {
		wlr_log_errno(WLR_ERROR,
			"Failed to bump max open files limit: getrlimit(NOFILE) failed");
		return;
	}

	struct rlimit new_rlimit = original_nofile_rlimit;
	new_rlimit.rlim_cur = new_rlimit.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &new_rlimit) != 0) {
		wlr_log_errno(WLR_ERROR,
			"Failed to bump max open files limit: setrlimit(NOFILE) failed");

		wlr_log(WLR_INFO, "Running with %d max open files",
			(int)original_nofile_rlimit.rlim_cur);
	}
}

void
restore_nofile_limit(void)
{
	if (original_nofile_rlimit.rlim_cur == 0) {
		return;
	}

	if (setrlimit(RLIMIT_NOFILE, &original_nofile_rlimit) != 0) {
		wlr_log_errno(WLR_ERROR,
			"Failed to restore max open files limit: setrlimit(NOFILE) failed");
	}
}
