/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OUTPUT_H
#define LABWC_OUTPUT_H

#include <wlr/types/wlr_output.h>
#include "common/edge.h"

#define LAB_NR_LAYERS (4)

struct output {
	struct wl_list link; /* server.outputs */
	struct server *server;
	struct wlr_output *wlr_output;
	struct wlr_output_state pending;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_tree *layer_tree[LAB_NR_LAYERS];
	struct wlr_scene_tree *layer_popup_tree;
	struct wlr_scene_tree *cycle_osd_tree;
	struct wlr_scene_tree *session_lock_tree;
	struct wlr_scene_buffer *workspace_osd;

	struct cycle_osd_scene {
		struct wl_list items; /* struct cycle_osd_item */
		struct wlr_scene_tree *tree;
	} cycle_osd;

	/* In output-relative scene coordinates */
	struct wlr_box usable_area;

	struct wl_list regions;  /* struct region.link */

	struct wl_listener destroy;
	struct wl_listener frame;
	struct wl_listener request_state;

	/*
	 * Unique power-of-two ID used in bitsets such as view->outputs.
	 * (This assumes there are never more than 64 outputs connected
	 * at once; wlr_scene_output has a similar limitation.)
	 *
	 * There's currently no attempt to maintain the same ID if the
	 * same physical output is disconnected and reconnected.
	 * However, IDs do get reused eventually if enough outputs are
	 * disconnected and connected again.
	 */
	uint64_t id_bit;

	bool gamma_lut_changed;
};

#undef LAB_NR_LAYERS

void output_init(struct server *server);
void output_finish(struct server *server);
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
	enum lab_edge edge, bool wrap);

bool output_is_usable(struct output *output);
void output_update_usable_area(struct output *output);
void output_update_all_usable_areas(struct server *server, bool layout_changed);
bool output_get_tearing_allowance(struct output *output);
struct wlr_box output_usable_area_in_layout_coords(struct output *output);
void handle_output_power_manager_set_mode(struct wl_listener *listener,
	void *data);
void output_enable_adaptive_sync(struct output *output, bool enabled);

/**
 * Notifies whether a fullscreen view is displayed on the given output.
 * Depending on user config, this may enable/disable adaptive sync.
 *
 * Does nothing if output is NULL or disabled.
 */
void output_set_has_fullscreen_view(struct output *output,
	bool has_fullscreen_view);

#endif // LABWC_OUTPUT_H
