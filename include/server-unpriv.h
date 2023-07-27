/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SERVER_UNPRIV_H
#define LABWC_SERVER_UNPRIV_H

struct server;
struct wl_client;

void unpriv_socket_start(struct server *server);
bool is_unpriv_client(const struct wl_client *wl_client);

#endif /* LABWC_UNPRIV_H */
