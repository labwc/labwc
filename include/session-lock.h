/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SESSION_LOCK_H
#define LABWC_SESSION_LOCK_H

#include <wlr/types/wlr_session_lock_v1.h>

struct output;
struct server;

struct session_lock_manager {
	struct server *server;
	struct wlr_session_lock_manager_v1 *wlr_manager;
	struct wlr_surface *focused;
	/*
	 * When not locked: lock=NULL, locked=false
	 * When locked: lock=non-NULL, locked=true
	 * When lock is destroyed without being unlocked: lock=NULL, locked=true
	 */
	struct wlr_session_lock_v1 *lock;
	bool locked;

	struct wl_list lock_outputs;

	struct wl_listener new_lock;
	struct wl_listener destroy;

	struct wl_listener lock_new_surface;
	struct wl_listener lock_unlock;
	struct wl_listener lock_destroy;
};

void session_lock_init(struct server *server);
void session_lock_output_create(struct session_lock_manager *manager, struct output *output);
void session_lock_update_for_layout_change(struct server *server);

#endif /* LABWC_SESSION_LOCK_H */
