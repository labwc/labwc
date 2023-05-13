/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DECORATIONS_H
#define LABWC_DECORATIONS_H

struct server;
struct view;
struct wlr_surface;

void kde_server_decoration_init(struct server *server);
void xdg_server_decoration_init(struct server *server);

void kde_server_decoration_update_default(void);
void kde_server_decoration_set_view(struct view *view, struct wlr_surface *surface);

#endif /* LABWC_DECORATIONS_H */
