/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ICON_LOADER_H
#define LABWC_ICON_LOADER_H

struct server;

void icon_loader_init(struct server *server);
void icon_loader_finish(struct server *server);
struct lab_data_buffer *icon_loader_lookup(struct server *server,
	const char *app_id, int size, float scale);

#endif /* LABWC_ICON_LOADER_H */
