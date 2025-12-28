// SPDX-License-Identifier: GPL-2.0-only
#include "cycle.h"
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/lab-scene-rect.h"
#include "common/list.h"
#include "common/mem.h"
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

static bool init_cycle(struct server *server, struct cycle_filter filter);
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

/* Returns the view to select next in the window switcher. */
static struct view *
get_next_selected_view(struct server *server, enum lab_cycle_dir dir)
{
	struct cycle_state *cycle = &server->cycle;
	assert(cycle->selected_view);
	assert(!wl_list_empty(&server->cycle.views));

	struct wl_list *link;
	if (dir == LAB_CYCLE_DIR_FORWARD) {
		link = cycle->selected_view->cycle_link.next;
		if (link == &server->cycle.views) {
			link = link->next;
		}
	} else {
		link = cycle->selected_view->cycle_link.prev;
		if (link == &server->cycle.views) {
			link = link->prev;
		}
	}
	struct view *view = wl_container_of(link, view, cycle_link);
	return view;
}

static struct view *
get_first_view(struct wl_list *views)
{
	assert(!wl_list_empty(views));
	struct view *view = wl_container_of(views->next, view, cycle_link);
	return view;
}

void
cycle_reinitialize(struct server *server)
{
	struct cycle_state *cycle = &server->cycle;

	if (server->input_mode != LAB_INPUT_STATE_CYCLE) {
		/* OSD not active, no need for clean up */
		return;
	}

	struct view *selected_view = cycle->selected_view;
	struct view *selected_view_prev =
		get_next_selected_view(server, LAB_CYCLE_DIR_BACKWARD);
	struct cycle_filter filter = cycle->filter;

	destroy_cycle(server);
	if (init_cycle(server, filter)) {
		/*
		 * Preserve the selected view (or its previous view) if it's
		 * still in the cycle list
		 */
		if (selected_view->cycle_link.next) {
			cycle->selected_view = selected_view;
		} else if (selected_view_prev->cycle_link.next) {
			cycle->selected_view = selected_view_prev;
		} else {
			/* should be unreachable */
			wlr_log(WLR_ERROR, "could not find view to select");
			cycle->selected_view = get_first_view(&server->cycle.views);
		}
		update_cycle(server);
	} else {
		/* Failed to re-init window switcher, exit */
		cycle_finish(server, /*switch_focus*/ false);
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
		server->cycle.preview_was_enabled = false;
		server->cycle.preview_was_shaded = false;
	}
}

void
cycle_begin(struct server *server, enum lab_cycle_dir direction,
		struct cycle_filter filter)
{
	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		return;
	}

	if (!init_cycle(server, filter)) {
		return;
	}

	struct view *active_view = server->active_view;
	if (active_view && active_view->cycle_link.next) {
		/* Select the active view it's in the cycle list */
		server->cycle.selected_view = active_view;
	} else {
		/* Otherwise, select the first view in the cycle list */
		server->cycle.selected_view = get_first_view(&server->cycle.views);
	}
	/* Pre-select the next view in the given direction */
	server->cycle.selected_view = get_next_selected_view(server, direction);

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

	server->cycle.selected_view = get_next_selected_view(server, direction);
	update_cycle(server);
}

void
cycle_finish(struct server *server, bool switch_focus)
{
	if (server->input_mode != LAB_INPUT_STATE_CYCLE) {
		return;
	}

	struct view *selected_view = server->cycle.selected_view;
	destroy_cycle(server);

	/* FIXME: this sets focus to the old surface even with switch_focus=true */
	seat_focus_override_end(&server->seat);

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
	switch (rc.window_switcher.osd.style) {
	case CYCLE_OSD_STYLE_CLASSIC:
		return &cycle_osd_classic_impl;
	case CYCLE_OSD_STYLE_THUMBNAIL:
		return &cycle_osd_thumbnail_impl;
	}
	return NULL;
}

static uint64_t
get_outputs_by_filter(struct server *server,
		enum cycle_output_filter output_filter)
{
	struct output *output = NULL;

	switch (output_filter) {
	case CYCLE_OUTPUT_ALL:
		break;
	case CYCLE_OUTPUT_CURSOR:
		output = output_nearest_to_cursor(server);
		break;
	case CYCLE_OUTPUT_FOCUSED: {
		struct view *view = server->active_view;
		if (view && output_is_usable(view->output)) {
			output = view->output;
		} else {
			/* Fallback to pointer */
			output = output_nearest_to_cursor(server);
		}
		break;
	}
	}

	if (output) {
		return output->id_bit;
	} else {
		/* bitmask for all outputs */
		return UINT64_MAX;
	}
}

static void
insert_view_ordered_by_age(struct wl_list *views, struct view *new_view)
{
	struct wl_list *link = views;
	struct view *view;
	wl_list_for_each(view, views, cycle_link) {
		if (view->creation_id >= new_view->creation_id) {
			break;
		}
		link = &view->cycle_link;
	}
	wl_list_insert(link, &new_view->cycle_link);
}

static void
handle_osd_tree_destroy(struct wl_listener *listener, void *data)
{
	struct cycle_osd_output *osd_output =
		wl_container_of(listener, osd_output, tree_destroy);
	struct cycle_osd_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &osd_output->items, link) {
		wl_list_remove(&item->link);
		free(item);
	}
	wl_list_remove(&osd_output->tree_destroy.link);
	wl_list_remove(&osd_output->link);
	free(osd_output);
}

/* Return false on failure */
static bool
init_cycle(struct server *server, struct cycle_filter filter)
{
	enum lab_view_criteria criteria =
		LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER
		| LAB_VIEW_CRITERIA_ROOT_TOPLEVEL;
	if (filter.workspace == CYCLE_WORKSPACE_CURRENT) {
		criteria |= LAB_VIEW_CRITERIA_CURRENT_WORKSPACE;
	}

	uint64_t cycle_outputs =
		get_outputs_by_filter(server, filter.output);

	const char *cycle_app_id = NULL;
	if (filter.app_id == CYCLE_APP_ID_CURRENT && server->active_view) {
		cycle_app_id = server->active_view->app_id;
	}

	struct view *view;
	for_each_view(view, &server->views, criteria) {
		if (!(cycle_outputs & view->output->id_bit)) {
			continue;
		}
		if (cycle_app_id && strcmp(view->app_id, cycle_app_id) != 0) {
			continue;
		}

		if (rc.window_switcher.order == WINDOW_SWITCHER_ORDER_AGE) {
			insert_view_ordered_by_age(&server->cycle.views, view);
		} else {
			wl_list_append(&server->cycle.views, &view->cycle_link);
		}
	}
	if (wl_list_empty(&server->cycle.views)) {
		wlr_log(WLR_DEBUG, "no views to switch between");
		return false;
	}
	server->cycle.filter = filter;

	if (rc.window_switcher.osd.show) {
		/* Create OSD */
		uint64_t osd_outputs = get_outputs_by_filter(server,
				rc.window_switcher.osd.output_filter);
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
			if (!(osd_outputs & output->id_bit)) {
				continue;
			}
			if (!output_is_usable(output)) {
				continue;
			}

			struct cycle_osd_output *osd_output = znew(*osd_output);
			wl_list_append(&server->cycle.osd_outputs, &osd_output->link);
			osd_output->output = output;
			wl_list_init(&osd_output->items);

			get_osd_impl()->init(osd_output);

			osd_output->tree_destroy.notify = handle_osd_tree_destroy;
			wl_signal_add(&osd_output->tree->node.events.destroy,
				&osd_output->tree_destroy);
		}
	}

	return true;
}

static void
update_cycle(struct server *server)
{
	struct cycle_state *cycle = &server->cycle;

	if (rc.window_switcher.osd.show) {
		struct cycle_osd_output *osd_output;
		wl_list_for_each(osd_output, &cycle->osd_outputs, link) {
			get_osd_impl()->update(osd_output);
		}
	}

	if (rc.window_switcher.preview) {
		preview_selected_view(cycle->selected_view);
	}

	/* Outline current window */
	if (rc.window_switcher.outlines) {
		if (view_is_focusable(cycle->selected_view)) {
			update_preview_outlines(cycle->selected_view);
		}
	}
}

/* Resets all the states in server->cycle */
static void
destroy_cycle(struct server *server)
{
	struct cycle_osd_output *osd_output, *tmp;
	wl_list_for_each_safe(osd_output, tmp, &server->cycle.osd_outputs, link) {
		/* calls handle_osd_tree_destroy() */
		wlr_scene_node_destroy(&osd_output->tree->node);
	}

	restore_preview_node(server);

	if (server->cycle.preview_outline) {
		wlr_scene_node_destroy(&server->cycle.preview_outline->tree->node);
		server->cycle.preview_outline = NULL;
	}

	struct view *view, *tmp2;
	wl_list_for_each_safe(view, tmp2, &server->cycle.views, cycle_link) {
		wl_list_remove(&view->cycle_link);
		view->cycle_link = (struct wl_list){0};
	}

	server->cycle.selected_view = NULL;
	server->cycle.filter = (struct cycle_filter){0};
}
