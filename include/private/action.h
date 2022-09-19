/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_PRIVATE_ACTION_H
#define __LABWC_PRIVATE_ACTION_H

/* Don't include ourself as search path starts at current directory */
#include "../action.h"

enum action_arg_type {
	LAB_ACTION_ARG_STR = 0,
};

struct action_arg {
	struct wl_list link;        /* struct action.args */

	char *key;                  /* May be NULL if there is just one arg */
	enum action_arg_type type;
};

struct action_arg_str {
	struct action_arg base;
	char *value;
};

#endif /* __LABWC_PRIVATE_ACTION_H */
