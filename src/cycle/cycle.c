// SPDX-License-Identifier: GPL-2.0-only
#include "cycle.h"
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/array.h"
#include "common/lab-scene-rect.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "scaled-buffer/scaled-icon-buffer.h"
#include "ssd.h"
#include "theme.h"
#include "view.h"

static void update_cycle(struct server *server);
static void destroy_cycle(struct server *server);

static void
update_preview_outlines(struct view *view)
{
	/* Create / Update preview outline tree */
	struct server *server = view->server;
	struct theme *theme = server->theme;
	struct lab_scene_rect *rect = view->server->cycle.preview_outline;
	if (!rect) {
		struct lab_scene_rect_options opts = {
			.border_colors = (float *[3]) {
				theme->osd_window_switcher_preview_border_color[0],
				theme->osd_window_switcher_preview_border_color[1],
				theme->osd_window_switcher_preview_border_color[2],
			},
			.nr_borders = 3,
			.border_width = theme->osd_window_switcher_preview_border_width,
		};
		rect = lab_scene_rect_create(&server->scene->tree, &opts);
		wlr_scene_node_place_above(&rect->tree->node, &server->menu_tree->node);
		server->cycle.preview_outline = rect;
	}

	struct wlr_box geo = ssd_max_extents(view);
	lab_scene_rect_set_size(rect, geo.width, geo.height);
	wlr_scene_node_set_position(&rect->tree->node, geo.x, geo.y);
}

/*
 * Returns the view to select next in the window switcher.
 * If !start_view, the second focusable view is returned.
 */
static struct view *
get_next_selected_view(struct server *server, struct view *start_view,
		enum lab_cycle_dir dir)
{
	struct view *(*iter)(struct wl_list *head, struct view *view,
		enum lab_view_criteria criteria);
	bool forwards = dir == LAB_CYCLE_DIR_FORWARD;
	iter = forwards ? view_next_no_head_stop : view_prev_no_head_stop;

	enum lab_view_criteria criteria = rc.window_switcher.criteria;

	/*
	 * Views are listed in stacking order, topmost first.  Usually the
	 * topmost view is already focused, so when iterating in the forward
	 * direction we pre-select the view second from the top:
	 *
	 *   View #1 (on top, currently focused)
	 *   View #2 (pre-selected)
	 *   View #3
	 *   ...
	 */
	if (!start_view && forwards) {
		start_view = iter(&server->views, NULL, criteria);
	}

	return iter(&server->views, start_view, criteria);
}

void
cycle_on_view_destroy(struct view *view)
{
	assert(view);
	struct server *server = view->server;
	struct cycle_state *cycle = &server->cycle;

	if (server->input_mode != LAB_INPUT_STATE_CYCLE) {
		/* OSD not active, no need for clean up */
		return;
	}

	if (cycle->selected_view == view) {
		/*
		 * If we are the current OSD selected view, cycle
		 * to the next because we are dying.
		 */

		/* Also resets preview node */
		cycle->selected_view = get_next_selected_view(server,
			cycle->selected_view, LAB_CYCLE_DIR_BACKWARD);

		/*
		 * If we cycled back to ourselves, then we have no more windows.
		 * Just close the OSD for good.
		 */
		if (cycle->selected_view == view
				|| !cycle->selected_view) {
			/* cycle_finish() additionally resets selected_view to NULL */
			cycle_finish(server, /*switch_focus*/ false);
		}
	}

	if (cycle->selected_view) {
		/* Recreate the OSD to reflect the view has now gone. */
		destroy_cycle(server);
		update_cycle(server);
	}
}

void
cycle_on_cursor_release(struct server *server, struct wlr_scene_node *node)
{
	assert(server->input_mode == LAB_INPUT_STATE_CYCLE);

	struct cycle_osd_item *item = node_cycle_osd_item_from_node(node);
	server->cycle.selected_view = item->view;
	cycle_finish(server, /*switch_focus*/ true);
}

static void
restore_preview_node(struct server *server)
{
	if (server->cycle.preview_node) {
		wlr_scene_node_reparent(server->cycle.preview_node,
			server->cycle.preview_dummy->parent);
		wlr_scene_node_place_above(server->cycle.preview_node,
			server->cycle.preview_dummy);
		wlr_scene_node_destroy(server->cycle.preview_dummy);

		/* Node was disabled / minimized before, disable again */
		if (!server->cycle.preview_was_enabled) {
			wlr_scene_node_set_enabled(server->cycle.preview_node, false);
		}
		if (server->cycle.preview_was_shaded) {
			struct view *view = node_view_from_node(server->cycle.preview_node);
			view_set_shade(view, true);
		}
		server->cycle.preview_node = NULL;
		server->cycle.preview_dummy = NULL;
		server->cycle.preview_was_shaded = false;
	}
}

void
cycle_begin(struct server *server, enum lab_cycle_dir direction)
{
	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		return;
	}

	server->cycle.selected_view = get_next_selected_view(server,
		server->cycle.selected_view, direction);

	seat_focus_override_begin(&server->seat,
		LAB_INPUT_STATE_CYCLE, LAB_CURSOR_DEFAULT);
	update_cycle(server);

	/* Update cursor, in case it is within the area covered by OSD */
	cursor_update_focus(server);
}

void
cycle_step(struct server *server, enum lab_cycle_dir direction)
{
	assert(server->input_mode == LAB_INPUT_STATE_CYCLE);

	server->cycle.selected_view = get_next_selected_view(server,
		server->cycle.selected_view, direction);
	update_cycle(server);
}

void
cycle_finish(struct server *server, bool switch_focus)
{
	if (server->input_mode != LAB_INPUT_STATE_CYCLE) {
		return;
	}

	restore_preview_node(server);
	/* FIXME: this sets focus to the old surface even with switch_focus=true */
	seat_focus_override_end(&server->seat);

	struct view *selected_view = server->cycle.selected_view;
	server->cycle.preview_node = NULL;
	server->cycle.preview_dummy = NULL;
	server->cycle.selected_view = NULL;
	server->cycle.preview_was_shaded = false;

	destroy_cycle(server);

	if (server->cycle.preview_outline) {
		/* Destroy the whole multi_rect so we can easily react to new themes */
		wlr_scene_node_destroy(&server->cycle.preview_outline->tree->node);
		server->cycle.preview_outline = NULL;
	}

	/* Hiding OSD may need a cursor change */
	cursor_update_focus(server);

	if (switch_focus && selected_view) {
		if (rc.window_switcher.unshade) {
			view_set_shade(selected_view, false);
		}
		desktop_focus_view(selected_view, /*raise*/ true);
	}
}

static void
preview_selected_view(struct view *view)
{
	assert(view);
	assert(view->scene_tree);
	struct server *server = view->server;
	struct cycle_state *cycle = &server->cycle;

	/* Move previous selected node back to its original place */
	restore_preview_node(server);

	cycle->preview_node = &view->scene_tree->node;

	/* Create a dummy node at the original place of the previewed window */
	struct wlr_scene_rect *dummy_rect = wlr_scene_rect_create(
		cycle->preview_node->parent, 0, 0, (float [4]) {0});
	wlr_scene_node_place_below(&dummy_rect->node, cycle->preview_node);
	wlr_scene_node_set_enabled(&dummy_rect->node, false);
	cycle->preview_dummy = &dummy_rect->node;

	/* Store node enabled / minimized state and force-enable if disabled */
	cycle->preview_was_enabled = cycle->preview_node->enabled;
	wlr_scene_node_set_enabled(cycle->preview_node, true);
	if (rc.window_switcher.unshade && view->shaded) {
		view_set_shade(view, false);
		cycle->preview_was_shaded = true;
	}

	/*
	 * FIXME: This abuses an implementation detail of the always-on-top tree.
	 *        Create a permanent server->osd_preview_tree instead that can
	 *        also be used as parent for the preview outlines.
	 */
	wlr_scene_node_reparent(cycle->preview_node,
		view->server->view_tree_always_on_top);

	/* Finally raise selected node to the top */
	wlr_scene_node_raise_to_top(cycle->preview_node);
}

static struct cycle_osd_impl *
get_osd_impl(void)
{
	switch (rc.window_switcher.style) {
	case CYCLE_OSD_STYLE_CLASSIC:
		return &cycle_osd_classic_impl;
	case CYCLE_OSD_STYLE_THUMBNAIL:
		return &cycle_osd_thumbnail_impl;
	}
	return NULL;
}

static void
update_osd_on_output(struct output *output, struct wl_array *views)
{
	if (!output_is_usable(output)) {
		return;
	}
	if (!output->cycle_osd.tree) {
		get_osd_impl()->create(output, views);
		assert(output->cycle_osd.tree);
	}
	get_osd_impl()->update(output);
}

static void
update_cycle(struct server *server)
{
	struct wl_array views;
	wl_array_init(&views);
	view_array_append(server, &views, rc.window_switcher.criteria);

	if (!wl_array_len(&views) || !server->cycle.selected_view) {
		cycle_finish(server, /*switch_focus*/ false);
		goto out;
	}

	if (rc.window_switcher.show) {
		/* Display the actual OSD */
		switch (rc.window_switcher.output_criteria) {
		case CYCLE_OSD_OUTPUT_ALL: {
			struct output *output;
			wl_list_for_each(output, &server->outputs, link) {
				update_osd_on_output(output, &views);
			}
			break;
		}
		case CYCLE_OSD_OUTPUT_POINTER:
			update_osd_on_output(output_nearest_to_cursor(server), &views);
			break;
		case CYCLE_OSD_OUTPUT_KEYBOARD: {
			struct output *output;
			if (server->active_view) {
				output = server->active_view->output;
			} else {
				/* Fallback to pointer, if there is no active_view */
				output = output_nearest_to_cursor(server);
			}
			update_osd_on_output(output, &views);
			break;
		}
		}
	}

	if (rc.window_switcher.preview) {
		preview_selected_view(server->cycle.selected_view);
	}

	/* Outline current window */
	if (rc.window_switcher.outlines) {
		if (view_is_focusable(server->cycle.selected_view)) {
			update_preview_outlines(server->cycle.selected_view);
		}
	}

out:
	wl_array_release(&views);
}

static void
destroy_cycle(struct server *server)
{
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct cycle_osd_item *item, *tmp;
		wl_list_for_each_safe(item, tmp, &output->cycle_osd.items, link) {
			wl_list_remove(&item->link);
			free(item);
		}
		if (output->cycle_osd.tree) {
			wlr_scene_node_destroy(&output->cycle_osd.tree->node);
			output->cycle_osd.tree = NULL;
		}
	}
}
