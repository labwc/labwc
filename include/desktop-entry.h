/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DESKTOP_ENTRY_H
#define LABWC_DESKTOP_ENTRY_H

struct server;

void desktop_entry_init(struct server *server);
void desktop_entry_finish(struct server *server);

struct lab_data_buffer *desktop_entry_icon_lookup(struct server *server,
	const char *app_id, int size, float scale);

/**
 * desktop_entry_name_lookup() - return the application name
 * from the sfdo desktop entry database based on app_id
 *
 * The lifetime of the returned value is the same as that
 * of sfdo_desktop_db (from `struct sfdo.desktop_db`)
 */
const char *desktop_entry_name_lookup(struct server *server,
	const char *app_id);

#endif /* LABWC_DESKTOP_ENTRY_H */
