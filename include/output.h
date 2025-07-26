/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OUTPUT_H
#define LABWC_OUTPUT_H

#include <wlr/types/wlr_output.h>

enum view_edge;

#define LAB_NR_LAYERS (4)

struct output {
	struct wl_list link; /* server.outputs */
	struct server *server;
	struct wlr_output *wlr_output;
	struct wlr_output_state pending;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_tree *layer_tree[LAB_NR_LAYERS];
	struct wlr_scene_tree *layer_popup_tree;
	struct wlr_scene_tree *osd_tree;
	struct wlr_scene_tree *session_lock_tree;
	struct wlr_scene_buffer *workspace_osd;

	struct osd_scene {
		struct wl_array items; /* struct osd_scene_item */
		struct wlr_scene_tree *tree;
	} osd_scene;

	/* In output-relative scene coordinates */
	struct wlr_box usable_area;

	struct wl_list regions;  /* struct region.link */

	struct wl_listener destroy;
	struct wl_listener frame;
	struct wl_listener request_state;

	bool gamma_lut_changed;
};

#undef LAB_NR_LAYERS

void output_init(struct server *server);
void output_finish(struct server *server);
void output_manager_init(struct server *server);
struct output *output_from_wlr_output(struct server *server,
	struct wlr_output *wlr_output);
struct output *output_from_name(struct server *server, const char *name);
struct output *output_nearest_to(struct server *server, int lx, int ly);
struct output *output_nearest_to_cursor(struct server *server);

/**
 * output_get_adjacent() - get next output, in a given direction,
 * from a given output
 *
 * @output: reference output
 * @edge: direction in which to look for the nearest output
 * @wrap: if true, wrap around at layout edge
 *
 * Note: if output is NULL, the output nearest the cursor will be used as the
 * reference instead.
 */
struct output *output_get_adjacent(struct output *output,
	enum view_edge edge, bool wrap);

bool output_is_usable(struct output *output);
void output_update_usable_area(struct output *output);
void output_update_all_usable_areas(struct server *server, bool layout_changed);
bool output_get_tearing_allowance(struct output *output);
struct wlr_box output_usable_area_in_layout_coords(struct output *output);
void handle_output_power_manager_set_mode(struct wl_listener *listener,
	void *data);
void output_enable_adaptive_sync(struct output *output, bool enabled);

/**
 * output_max_scale() - get maximum scale factor of all usable outputs.
 * Used when loading/rendering resources (e.g. icons) that may be
 * displayed on any output.
 */
float output_max_scale(struct server *server);

#endif // LABWC_OUTPUT_H
