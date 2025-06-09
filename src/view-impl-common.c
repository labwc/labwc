// SPDX-License-Identifier: GPL-2.0-only
/* view-impl-common.c: common code for shell view->impl functions */
#include <stdio.h>
#include <strings.h>
#include <math.h>
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include "foreign-toplevel.h"
#include "labwc.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"
#include "workspaces.h"

static inline uint64_t
timespec_to_us(const struct timespec *ts)
{
	return (uint64_t)ts->tv_sec * UINT64_C(1000000) +
		(uint64_t)ts->tv_nsec / UINT64_C(1000);
}

static inline uint64_t
gettime_us(void)
{
	struct timespec ts = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return timespec_to_us(&ts);
}

static void
adjust_scaled_position(struct view *view, double scale)
{
	int x = view->current.x;
	int y = view->current.y;
	double width = view->current.width;
	double height = view->current.height;

	int x_pos = x + round(width * (1.0 - scale) / 2.0);
	int y_pos = y + round(height * (1.0 - scale) / 2.0);

	wlr_scene_node_set_position(&view->scene_tree->node, x_pos, y_pos);
}

static void
view_run_animation(struct view *view, uint64_t now)
{
	uint64_t elapsed_us = now - view->animation_start_time;
	double t = (double)elapsed_us * 1e-6;
	double duration = 0.2; // s
	double scale = fmax(t / duration, 0.01);

	if (scale < 1.0) {
		wlr_scene_node_set_scale(&view->scene_tree->node, scale);
		adjust_scaled_position(view, scale);
	} else {
		int x = view->current.x;
		int y = view->current.y;
		wlr_scene_node_set_position(&view->scene_tree->node, x, y);
		wlr_scene_node_set_scale(&view->scene_tree->node, 1.0);
		wl_list_remove(&view->output_commit.link);
		wl_list_init(&view->output_commit.link);
		view->animation_start_time = 0;
	}
}

static void
handle_output_commit(struct wl_listener *listener, void *data)
{
	struct wlr_output_event_commit *event = data;
	struct view *view = wl_container_of(listener, view, output_commit);

	if (!(event->state->committed & WLR_OUTPUT_STATE_BUFFER)) {
		return;
	}

	view_run_animation(view, gettime_us());

	wlr_output_update_needs_frame(event->output);
}

static void
view_start_animation(struct view *view)
{
	view->output_commit.notify = handle_output_commit;
	wl_signal_add(&view->output->wlr_output->events.commit, &view->output_commit);

	uint64_t now = gettime_us();
	view->animation_start_time = now;
	view_run_animation(view, now);
}

void
view_impl_map(struct view *view)
{
	desktop_focus_view(view, /*raise*/ true);
	view_update_title(view);
	view_update_app_id(view);
	if (!view->been_mapped) {
		window_rules_apply(view, LAB_WINDOW_RULE_EVENT_ON_FIRST_MAP);
		view_start_animation(view);
	}

	/*
	 * It's tempting to just never create the foreign-toplevel handle in the
	 * map handlers, but the app_id/title might not have been set at that
	 * point, so it's safer to process the property here
	 */
	enum property ret = window_rules_get_property(view, "skipTaskbar");
	if (ret == LAB_PROP_TRUE) {
		if (view->foreign_toplevel) {
			foreign_toplevel_destroy(view->foreign_toplevel);
			view->foreign_toplevel = NULL;
		}
	}

	/*
	 * Some clients (e.g. Steam's Big Picture Mode window) request
	 * fullscreen before mapping.
	 */
	desktop_update_top_layer_visibility(view->server);

	wlr_log(WLR_DEBUG, "[map] identifier=%s, title=%s",
		view_get_string_prop(view, "app_id"),
		view_get_string_prop(view, "title"));
}

void
view_impl_unmap(struct view *view)
{
	struct server *server = view->server;
	if (view == server->active_view) {
		desktop_focus_topmost_view(server);
	}
}

static bool
resizing_edge(struct view *view, uint32_t edge)
{
	struct server *server = view->server;
	return server->input_mode == LAB_INPUT_STATE_RESIZE
		&& server->grabbed_view == view
		&& (server->resize_edges & edge);
}

void
view_impl_apply_geometry(struct view *view, int w, int h)
{
	struct wlr_box *current = &view->current;
	struct wlr_box *pending = &view->pending;
	struct wlr_box old = *current;

	/*
	 * Anchor right edge if resizing via left edge.
	 *
	 * Note that answering the question "are we resizing?" is a bit
	 * tricky. The most obvious method is to look at the server
	 * flags; but that method will not account for any late commits
	 * that occur after the mouse button is released, as the client
	 * catches up with pending configure requests. So as a fallback,
	 * we resort to a geometry-based heuristic -- also not 100%
	 * reliable on its own. The combination of the two methods
	 * should catch 99% of resize cases that we care about.
	 */
	bool resizing_left_edge = resizing_edge(view, WLR_EDGE_LEFT);
	if (resizing_left_edge || (current->x != pending->x
			&& current->x + current->width ==
			pending->x + pending->width)) {
		current->x = pending->x + pending->width - w;
	} else {
		current->x = pending->x;
	}

	/* Anchor bottom edge if resizing via top edge */
	bool resizing_top_edge = resizing_edge(view, WLR_EDGE_TOP);
	if (resizing_top_edge || (current->y != pending->y
			&& current->y + current->height ==
			pending->y + pending->height)) {
		current->y = pending->y + pending->height - h;
	} else {
		current->y = pending->y;
	}

	current->width = w;
	current->height = h;

	if (!wlr_box_equal(current, &old)) {
		view_moved(view);
	}

	if (view->animation_start_time) {
		adjust_scaled_position(view, view->scene_tree->node.scale);
	}
}
