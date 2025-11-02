// SPDX-License-Identifier: GPL-2.0-only
#include "osd.h"
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

static void update_osd(struct server *server);

static void
destroy_osd_scenes(struct server *server)
{
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->osd_scene.tree) {
			wlr_scene_node_destroy(&output->osd_scene.tree->node);
			output->osd_scene.tree = NULL;
		}
		wl_array_release(&output->osd_scene.items);
		wl_array_init(&output->osd_scene.items);
	}
}

static void
osd_update_preview_outlines(struct view *view)
{
	/* Create / Update preview outline tree */
	struct server *server = view->server;
	struct theme *theme = server->theme;
	struct lab_scene_rect *rect = view->server->osd_state.preview_outline;
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
		server->osd_state.preview_outline = rect;
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
get_next_cycle_view(struct server *server, struct view *start_view,
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
osd_on_view_destroy(struct view *view)
{
	assert(view);
	struct osd_state *osd_state = &view->server->osd_state;

	if (view->server->input_mode != LAB_INPUT_STATE_WINDOW_SWITCHER) {
		/* OSD not active, no need for clean up */
		return;
	}

	if (osd_state->cycle_view == view) {
		/*
		 * If we are the current OSD selected view, cycle
		 * to the next because we are dying.
		 */

		/* Also resets preview node */
		osd_state->cycle_view = get_next_cycle_view(view->server,
			osd_state->cycle_view, LAB_CYCLE_DIR_BACKWARD);

		/*
		 * If we cycled back to ourselves, then we have no more windows.
		 * Just close the OSD for good.
		 */
		if (osd_state->cycle_view == view || !osd_state->cycle_view) {
			/* osd_finish() additionally resets cycle_view to NULL */
			osd_finish(view->server);
		}
	}

	if (osd_state->cycle_view) {
		/* Recreate the OSD to reflect the view has now gone. */
		destroy_osd_scenes(view->server);
		update_osd(view->server);
	}

	if (view->scene_tree) {
		struct wlr_scene_node *node = &view->scene_tree->node;
		if (osd_state->preview_anchor == node) {
			/*
			 * If we are the anchor for the current OSD selected view,
			 * replace the anchor with the node before us.
			 */
			osd_state->preview_anchor = lab_wlr_scene_get_prev_node(node);
		}
	}
}

static void
restore_preview_node(struct server *server)
{
	struct osd_state *osd_state = &server->osd_state;
	if (osd_state->preview_node) {
		wlr_scene_node_reparent(osd_state->preview_node,
			osd_state->preview_parent);

		if (osd_state->preview_anchor) {
			wlr_scene_node_place_above(osd_state->preview_node,
				osd_state->preview_anchor);
		} else {
			/* Selected view was the first node */
			wlr_scene_node_lower_to_bottom(osd_state->preview_node);
		}

		/* Node was disabled / minimized before, disable again */
		if (!osd_state->preview_was_enabled) {
			wlr_scene_node_set_enabled(osd_state->preview_node, false);
		}
		if (osd_state->preview_was_shaded) {
			struct view *view = node_view_from_node(osd_state->preview_node);
			view_set_shade(view, true);
		}
		osd_state->preview_node = NULL;
		osd_state->preview_parent = NULL;
		osd_state->preview_anchor = NULL;
		osd_state->preview_was_shaded = false;
	}
}

void
osd_begin(struct server *server, enum lab_cycle_dir direction)
{
	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		return;
	}

	server->osd_state.cycle_view = get_next_cycle_view(server,
		server->osd_state.cycle_view, direction);

	seat_focus_override_begin(&server->seat,
		LAB_INPUT_STATE_WINDOW_SWITCHER, LAB_CURSOR_DEFAULT);
	update_osd(server);

	/* Update cursor, in case it is within the area covered by OSD */
	cursor_update_focus(server);
}

void
osd_cycle(struct server *server, enum lab_cycle_dir direction)
{
	assert(server->input_mode == LAB_INPUT_STATE_WINDOW_SWITCHER);

	server->osd_state.cycle_view = get_next_cycle_view(server,
		server->osd_state.cycle_view, direction);
	update_osd(server);
}

void
osd_finish(struct server *server)
{
	restore_preview_node(server);
	seat_focus_override_end(&server->seat);

	server->osd_state.preview_node = NULL;
	server->osd_state.preview_anchor = NULL;
	server->osd_state.cycle_view = NULL;
	server->osd_state.preview_was_shaded = false;

	destroy_osd_scenes(server);

	if (server->osd_state.preview_outline) {
		/* Destroy the whole multi_rect so we can easily react to new themes */
		wlr_scene_node_destroy(&server->osd_state.preview_outline->tree->node);
		server->osd_state.preview_outline = NULL;
	}

	/* Hiding OSD may need a cursor change */
	cursor_update_focus(server);
}

static void
preview_cycled_view(struct view *view)
{
	assert(view);
	assert(view->scene_tree);
	struct osd_state *osd_state = &view->server->osd_state;

	/* Move previous selected node back to its original place */
	restore_preview_node(view->server);

	/* Store some pointers so we can reset the preview later on */
	osd_state->preview_node = &view->scene_tree->node;
	osd_state->preview_parent = view->scene_tree->node.parent;

	/* Remember the sibling right before the selected node */
	osd_state->preview_anchor = lab_wlr_scene_get_prev_node(
		osd_state->preview_node);
	while (osd_state->preview_anchor && !osd_state->preview_anchor->data) {
		/* Ignore non-view nodes */
		osd_state->preview_anchor = lab_wlr_scene_get_prev_node(
			osd_state->preview_anchor);
	}

	/* Store node enabled / minimized state and force-enable if disabled */
	osd_state->preview_was_enabled = osd_state->preview_node->enabled;
	if (!osd_state->preview_was_enabled) {
		wlr_scene_node_set_enabled(osd_state->preview_node, true);
	}
	if (rc.window_switcher.unshade && view->shaded) {
		view_set_shade(view, false);
		osd_state->preview_was_shaded = true;
	}

	/*
	 * FIXME: This abuses an implementation detail of the always-on-top tree.
	 *        Create a permanent server->osd_preview_tree instead that can
	 *        also be used as parent for the preview outlines.
	 */
	wlr_scene_node_reparent(osd_state->preview_node,
		view->server->view_tree_always_on_top);

	/* Finally raise selected node to the top */
	wlr_scene_node_raise_to_top(osd_state->preview_node);
}

static void
update_osd(struct server *server)
{
	struct wl_array views;
	wl_array_init(&views);
	view_array_append(server, &views, rc.window_switcher.criteria);

	struct osd_impl *osd_impl = NULL;
	switch (rc.window_switcher.style) {
	case WINDOW_SWITCHER_CLASSIC:
		osd_impl = &osd_classic_impl;
		break;
	case WINDOW_SWITCHER_THUMBNAIL:
		osd_impl = &osd_thumbnail_impl;
		break;
	}

	if (!wl_array_len(&views) || !server->osd_state.cycle_view) {
		osd_finish(server);
		goto out;
	}

	if (rc.window_switcher.show) {
		/* Display the actual OSD */
		// struct output *output;
		// wl_list_for_each(output, &server->outputs, link) {
		// 	if (!output_is_usable(output)) {
		// 		continue;
		// 	}
		// 	if (!output->osd_scene.tree) {
		// 		osd_impl->create(output, &views);
		// 		assert(output->osd_scene.tree);
		// 	}
		// 	osd_impl->update(output);
		// }

		struct output *output = output_nearest_to_cursor(server);
		if (output_is_usable(output)) {
			if (!output->osd_scene.tree) {
				osd_impl->create(output, &views);
				assert(output->osd_scene.tree);
			}
			osd_impl->update(output);
		}
	}

	if (rc.window_switcher.preview) {
		preview_cycled_view(server->osd_state.cycle_view);
	}

	/* Outline current window */
	if (rc.window_switcher.outlines) {
		if (view_is_focusable(server->osd_state.cycle_view)) {
			osd_update_preview_outlines(server->osd_state.cycle_view);
		}
	}

out:
	wl_array_release(&views);
}
