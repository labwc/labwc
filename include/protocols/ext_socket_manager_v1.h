/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_EXT_SOCKET_MANAGER_H
#define LABWC_PROTOCOLS_EXT_SOCKET_MANAGER_H

#include <wayland-server-core.h>

struct ext_socket_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
		struct wl_signal register_socket;
	} events;

	struct wl_listener display_destroy;
};

struct ext_socket_manager_v1 *ext_socket_manager_v1_create(
	struct wl_display *display);

#endif
