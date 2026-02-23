/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DESKTOP_ENTRY_H
#define LABWC_DESKTOP_ENTRY_H
#include "config.h"
#if HAVE_LIBSFDO

struct server;

void desktop_entry_init(void);
void desktop_entry_finish(void);

struct lab_img *desktop_entry_load_icon_from_app_id(const char *app_id, int size, float scale);

struct lab_img *desktop_entry_load_icon(const char *icon_name, int size, float scale);

/**
 * desktop_entry_name_lookup() - return the application name
 * from the sfdo desktop entry database based on app_id
 *
 * The lifetime of the returned value is the same as that
 * of sfdo_desktop_db (from `struct sfdo.desktop_db`)
 */
const char *desktop_entry_name_lookup(const char *app_id);

#endif /* HAVE_LIBSFDO */
#endif /* LABWC_DESKTOP_ENTRY_H */
