/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PERMISSIONS_H
#define LABWC_PERMISSIONS_H

#include <wayland-server-core.h>
#include <stdbool.h>

uint32_t permissions_from_interface_name(const char *s);
int permissions_context_create(struct wl_display *display, uint32_t permissions);
bool permissions_check(const struct wl_client *client, const struct wl_interface *iface);

#endif
