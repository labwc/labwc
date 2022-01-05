/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_ACTION_H
#define __LABWC_ACTION_H

struct action {
	uint32_t type;
	char *arg;
	struct wl_list link;
};

struct action *action_create(const char *action_name);

#endif
