/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_CYCLE_H
#define LABWC_CYCLE_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>
#include "config/types.h"

struct output;
struct wlr_box;

enum lab_cycle_dir {
	LAB_CYCLE_DIR_NONE,
	LAB_CYCLE_DIR_FORWARD,
	LAB_CYCLE_DIR_BACKWARD,
};

/* TODO: add field with keyboard layout? */
enum cycle_osd_field_content {
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

struct cycle_osd_field {
	enum cycle_osd_field_content content;
	int width;
	char *format;
	struct wl_list link; /* struct rcxml.window_switcher.osd.fields */
};

struct cycle_filter {
	enum cycle_workspace_filter workspace;
	enum cycle_output_filter output;
	enum cycle_app_id_filter app_id;
	enum cycle_window_filter window;
};

struct cycle_state {
	struct view *selected_view;
	struct wl_list views;
	struct wl_list osd_outputs; /* struct cycle_osd_output.link */
	bool preview_was_shaded;
	bool preview_was_enabled;
	struct wlr_scene_node *preview_node;
	struct wlr_scene_node *preview_dummy;
	struct lab_scene_rect *preview_outline;
	struct cycle_filter filter;
};

struct cycle_osd_output {
	struct wl_list link; /* struct cycle_state.osd_outputs */
	struct output *output;
	struct wl_listener tree_destroy;

	/* set by cycle_osd_impl->init() */
	struct wl_list items; /* struct cycle_osd_item.link */
	struct wlr_scene_tree *tree;
	/* set by cycle_osd_impl->init() and moved by cycle_osd_scroll_update() */
	struct wlr_scene_tree *items_tree;

	/* used in osd-scroll.c */
	struct cycle_osd_scroll_context {
		int top_row_idx;
		int nr_rows, nr_cols, nr_visible_rows;
		int delta_y;
		struct wlr_box bar_area;
		struct wlr_scene_tree *bar_tree;
		struct lab_scene_rect *bar;
	} scroll;
};

struct buf;
struct view;
struct server;
struct wlr_scene_node;

/* Begin window switcher */
void cycle_begin(struct server *server, enum lab_cycle_dir direction,
	struct cycle_filter filter);

/* Cycle the selected view in the window switcher */
void cycle_step(struct server *server, enum lab_cycle_dir direction);

/* Closes the OSD */
void cycle_finish(struct server *server, bool switch_focus);

/* Re-initialize the window switcher */
void cycle_reinitialize(struct server *server);

/* Focus the clicked window and close OSD */
void cycle_on_cursor_release(struct server *server, struct wlr_scene_node *node);

/* Used by osd.c internally to render window switcher fields */
void cycle_osd_field_get_content(struct cycle_osd_field *field,
	struct buf *buf, struct view *view);
/* Sets view info to buf according to format */
void cycle_osd_field_set_custom(struct buf *buf, struct view *view,
	const char *format);

/* Used by rcxml.c when parsing the config */
void cycle_osd_field_arg_from_xml_node(struct cycle_osd_field *field,
	const char *nodename, const char *content);
bool cycle_osd_field_is_valid(struct cycle_osd_field *field);
void cycle_osd_field_free(struct cycle_osd_field *field);

/* Internal API */
struct cycle_osd_item {
	struct view *view;
	struct wlr_scene_tree *tree;
	struct wl_list link;
};

struct cycle_osd_impl {
	/*
	 * Create a scene-tree of OSD for an output and fill
	 * osd_output->items.
	 */
	void (*init)(struct cycle_osd_output *osd_output);
	/*
	 * Update the OSD to highlight server->cycle.selected_view.
	 */
	void (*update)(struct cycle_osd_output *osd_output);
};

#define SCROLLBAR_W 10

/**
 * Initialize the context and scene for scrolling OSD items.
 *
 * @output: Output of the OSD
 * @bar_area: Area where the scrollbar is drawn
 * @delta_y: The vertical delta by which items are scrolled (usually item height)
 * @nr_cols: Number of columns in the OSD
 * @nr_rows: Number of rows in the OSD
 * @nr_visible_rows: Number of visible rows in the OSD
 * @border_color: Border color of the scrollbar
 * @bg_color: Background color of the scrollbar
 */
void cycle_osd_scroll_init(struct cycle_osd_output *osd_output,
	struct wlr_box bar_area, int delta_y,
	int nr_cols, int nr_rows, int nr_visible_rows,
	float *border_color, float *bg_color);

/* Scroll the OSD to show server->cycle.selected_view if needed */
void cycle_osd_scroll_update(struct cycle_osd_output *osd_output);

extern struct cycle_osd_impl cycle_osd_classic_impl;
extern struct cycle_osd_impl cycle_osd_thumbnail_impl;

#endif // LABWC_CYCLE_H
