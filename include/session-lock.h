/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LAB_SESSION_LOCK_H
#define __LAB_SESSION_LOCK_H

#include <wlr/types/wlr_session_lock_v1.h>

struct session_lock {
	struct wlr_session_lock_v1 *lock;
	struct wlr_surface *focused;
	bool abandoned;

	struct wl_list session_lock_outputs;

	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
};

void session_lock_init(struct server *server);
void session_lock_output_create(struct session_lock *lock, struct output *output);

#endif /* __LAB_SESSION_LOCK_H */
