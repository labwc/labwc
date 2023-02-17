/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_XDG_UNMANAGED
#define __LABWC_XDG_UNMANAGED

struct server;
struct wlr_xdg_surface;

void xdg_unmanaged_create(struct server *server, struct wlr_xdg_surface *wlr_xdg_surface);

#endif /* __LABWC_XDG_UNMANAGED */
