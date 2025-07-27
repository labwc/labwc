/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OSD_H
#define LABWC_OSD_H

#include <stdbool.h>
#include <wayland-server-core.h>

/* TODO: add field with keyboard layout? */
enum window_switcher_field_content {
	LAB_FIELD_NONE = 0,
	LAB_FIELD_TYPE,
	LAB_FIELD_TYPE_SHORT,
	LAB_FIELD_IDENTIFIER,
	LAB_FIELD_TRIMMED_IDENTIFIER,
	LAB_FIELD_ICON,
	LAB_FIELD_DESKTOP_ENTRY_NAME,
	LAB_FIELD_TITLE,
	LAB_FIELD_TITLE_SHORT,
	LAB_FIELD_WORKSPACE,
	LAB_FIELD_WORKSPACE_SHORT,
	LAB_FIELD_WIN_STATE,
	LAB_FIELD_WIN_STATE_ALL,
	LAB_FIELD_OUTPUT,
	LAB_FIELD_OUTPUT_SHORT,
	LAB_FIELD_CUSTOM,

	LAB_FIELD_COUNT
};

struct window_switcher_field {
	enum window_switcher_field_content content;
	int width;
	char *format;
	struct wl_list link; /* struct rcxml.window_switcher.fields */
};

struct buf;
struct view;
struct server;
enum lab_cycle_dir;

/* Begin window switcher */
void osd_begin(struct server *server, enum lab_cycle_dir direction);

/* Cycle the selected view in the window switcher */
void osd_cycle(struct server *server, enum lab_cycle_dir direction);

/* Closes the OSD */
void osd_finish(struct server *server);

/* Notify OSD about a destroying view */
void osd_on_view_destroy(struct view *view);

/* Used by osd.c internally to render window switcher fields */
void osd_field_get_content(struct window_switcher_field *field,
	struct buf *buf, struct view *view);

/* Used by rcxml.c when parsing the config */
void osd_field_arg_from_xml_node(struct window_switcher_field *field,
	const char *nodename, const char *content);
bool osd_field_is_valid(struct window_switcher_field *field);
void osd_field_free(struct window_switcher_field *field);

#endif // LABWC_OSD_H
