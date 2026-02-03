// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "workspaces.h"
#include <assert.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/list.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "output.h"
#include "protocols/cosmic-workspaces.h"
#include "protocols/ext-workspace.h"
#include "theme.h"
#include "view.h"

#define COSMIC_WORKSPACES_VERSION 1
#define EXT_WORKSPACES_VERSION 1

/* Internal helpers */
static size_t
parse_workspace_index(const char *name)
{
	/*
	 * We only want to get positive numbers which span the whole string.
	 *
	 * More detailed requirement:
	 *  .---------------.--------------.
	 *  |     Input     | Return value |
	 *  |---------------+--------------|
	 *  | "2nd desktop" |      0       |
	 *  |    "-50"      |      0       |
	 *  |     "0"       |      0       |
	 *  |    "124"      |     124      |
	 *  |    "1.24"     |      0       |
	 *  `------------------------------´
	 *
	 * As atoi() happily parses any numbers until it hits a non-number we
	 * can't really use it for this case. Instead, we use strtol() combined
	 * with further checks for the endptr (remaining non-number characters)
	 * and returned negative numbers.
	 */
	long index;
	char *endptr;
	errno = 0;
	index = strtol(name, &endptr, 10);
	if (errno || *endptr != '\0' || index < 0) {
		return 0;
	}
	return index;
}

static void
_osd_update(struct server *server)
{
	struct theme *theme = server->theme;

	/* Settings */
	uint16_t margin = 10;
	uint16_t padding = 2;
	uint16_t rect_height = theme->osd_workspace_switcher_boxes_height;
	uint16_t rect_width = theme->osd_workspace_switcher_boxes_width;
	bool hide_boxes = theme->osd_workspace_switcher_boxes_width == 0 ||
		theme->osd_workspace_switcher_boxes_height == 0;

	/* Dimensions */
	size_t workspace_count = wl_list_length(&server->workspaces.all);
	uint16_t marker_width = workspace_count * (rect_width + padding) - padding;
	uint16_t width = margin * 2 + (marker_width < 200 ? 200 : marker_width);
	uint16_t height = margin * (hide_boxes ? 2 : 3) + rect_height + font_height(&rc.font_osd);

	cairo_t *cairo;
	cairo_surface_t *surface;
	struct workspace *workspace;

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (!output_is_usable(output)) {
			continue;
		}
		struct lab_data_buffer *buffer = buffer_create_cairo(width, height,
			output->wlr_output->scale);
		if (!buffer) {
			wlr_log(WLR_ERROR, "Failed to allocate buffer for workspace OSD");
			continue;
		}

		cairo = cairo_create(buffer->surface);

		/* Background */
		set_cairo_color(cairo, theme->osd_bg_color);
		cairo_rectangle(cairo, 0, 0, width, height);
		cairo_fill(cairo);

		/* Border */
		set_cairo_color(cairo, theme->osd_border_color);
		struct wlr_fbox border_fbox = {
			.width = width,
			.height = height,
		};
		draw_cairo_border(cairo, border_fbox, theme->osd_border_width);

		/* Boxes */
		uint16_t x;
		if (!hide_boxes) {
			x = (width - marker_width) / 2;
			wl_list_for_each(workspace, &server->workspaces.all, link) {
				bool active =  workspace == server->workspaces.current;
				set_cairo_color(cairo, server->theme->osd_label_text_color);
				struct wlr_fbox fbox = {
					.x = x,
					.y = margin,
					.width = rect_width,
					.height = rect_height,
				};
				draw_cairo_border(cairo, fbox,
					theme->osd_workspace_switcher_boxes_border_width);
				if (active) {
					cairo_rectangle(cairo, x, margin,
						rect_width, rect_height);
					cairo_fill(cairo);
				}
				x += rect_width + padding;
			}
		}

		/* Text */
		set_cairo_color(cairo, server->theme->osd_label_text_color);
		PangoLayout *layout = pango_cairo_create_layout(cairo);
		pango_context_set_round_glyph_positions(pango_layout_get_context(layout), false);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

		/* Center workspace indicator on the x axis */
		int req_width = font_width(&rc.font_osd, server->workspaces.current->name);
		req_width = MIN(req_width, width - 2 * margin);
		x = (width - req_width) / 2;
		if (!hide_boxes) {
			cairo_move_to(cairo, x, margin * 2 + rect_height);
		} else {
			cairo_move_to(cairo, x, (height - font_height(&rc.font_osd)) / 2.0);
		}
		PangoFontDescription *desc = font_to_pango_desc(&rc.font_osd);
		//pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_width(layout, req_width * PANGO_SCALE);
		pango_font_description_free(desc);
		pango_layout_set_text(layout, server->workspaces.current->name, -1);
		pango_cairo_show_layout(cairo, layout);

		g_object_unref(layout);
		surface = cairo_get_target(cairo);
		cairo_surface_flush(surface);
		cairo_destroy(cairo);

		if (!output->workspace_osd) {
			output->workspace_osd = wlr_scene_buffer_create(
				&server->scene->tree, NULL);
		}
		/* Position the whole thing */
		struct wlr_box output_box;
		wlr_output_layout_get_box(output->server->output_layout,
			output->wlr_output, &output_box);
		int lx = output_box.x + (output_box.width - width) / 2;
		int ly = output_box.y + (output_box.height - height) / 2;
		wlr_scene_node_set_position(&output->workspace_osd->node, lx, ly);
		wlr_scene_buffer_set_buffer(output->workspace_osd, &buffer->base);
		wlr_scene_buffer_set_dest_size(output->workspace_osd,
			buffer->logical_width, buffer->logical_height);

		/* And finally drop the buffer so it will get destroyed on OSD hide */
		wlr_buffer_drop(&buffer->base);
	}
}

static struct workspace *
workspace_find_by_name(struct server *server, const char *name)
{
	struct workspace *workspace;

	/* by index */
	size_t parsed_index = parse_workspace_index(name);
	if (parsed_index) {
		size_t index = 0;
		wl_list_for_each(workspace, &server->workspaces.all, link) {
			if (parsed_index == ++index) {
				return workspace;
			}
		}
	}

	/* by name */
	wl_list_for_each(workspace, &server->workspaces.all, link) {
		if (!strcmp(workspace->name, name)) {
			return workspace;
		}
	}

	wlr_log(WLR_ERROR, "Workspace '%s' not found", name);
	return NULL;
}

/* cosmic workspace handlers */
static void
handle_cosmic_workspace_activate(struct wl_listener *listener, void *data)
{
	struct workspace *workspace = wl_container_of(listener, workspace, on_cosmic.activate);
	workspaces_switch_to(workspace, /* update_focus */ true);
	wlr_log(WLR_INFO, "cosmic activating workspace %s", workspace->name);
}

/* ext workspace handlers */
static void
handle_ext_workspace_activate(struct wl_listener *listener, void *data)
{
	struct workspace *workspace = wl_container_of(listener, workspace, on_ext.activate);
	workspaces_switch_to(workspace, /* update_focus */ true);
	wlr_log(WLR_INFO, "ext activating workspace %s", workspace->name);
}

/* Internal API */
static void
add_workspace(struct server *server, const char *name)
{
	struct workspace *workspace = znew(*workspace);
	workspace->server = server;
	workspace->name = xstrdup(name);
	workspace->tree = wlr_scene_tree_create(server->workspace_tree);
	workspace->view_trees[VIEW_LAYER_ALWAYS_ON_BOTTOM] =
		wlr_scene_tree_create(workspace->tree);
	workspace->view_trees[VIEW_LAYER_NORMAL] =
		wlr_scene_tree_create(workspace->tree);
	workspace->view_trees[VIEW_LAYER_ALWAYS_ON_TOP] =
		wlr_scene_tree_create(workspace->tree);
	wl_list_append(&server->workspaces.all, &workspace->link);
	wlr_scene_node_set_enabled(&workspace->tree->node, false);

	/* cosmic */
	workspace->cosmic_workspace = lab_cosmic_workspace_create(server->workspaces.cosmic_group);
	lab_cosmic_workspace_set_name(workspace->cosmic_workspace, name);

	workspace->on_cosmic.activate.notify = handle_cosmic_workspace_activate;
	wl_signal_add(&workspace->cosmic_workspace->events.activate,
		&workspace->on_cosmic.activate);

	/* ext */
	workspace->ext_workspace = lab_ext_workspace_create(
		server->workspaces.ext_manager, /*id*/ NULL);
	lab_ext_workspace_assign_to_group(workspace->ext_workspace, server->workspaces.ext_group);
	lab_ext_workspace_set_name(workspace->ext_workspace, name);

	workspace->on_ext.activate.notify = handle_ext_workspace_activate;
	wl_signal_add(&workspace->ext_workspace->events.activate,
		&workspace->on_ext.activate);
}

static struct workspace *
get_prev(struct workspace *current, struct wl_list *workspaces, bool wrap)
{
	struct wl_list *target_link = current->link.prev;
	if (target_link == workspaces) {
		/* Current workspace is the first one */
		if (!wrap) {
			return NULL;
		}
		/* Roll over */
		target_link = target_link->prev;
	}
	return wl_container_of(target_link, current, link);
}

static struct workspace *
get_next(struct workspace *current, struct wl_list *workspaces, bool wrap)
{
	struct wl_list *target_link = current->link.next;
	if (target_link == workspaces) {
		/* Current workspace is the last one */
		if (!wrap) {
			return NULL;
		}
		/* Roll over */
		target_link = target_link->next;
	}
	return wl_container_of(target_link, current, link);
}

static bool
workspace_has_views(struct workspace *workspace, struct server *server)
{
	struct view *view;

	for_each_view(view, &server->views, LAB_VIEW_CRITERIA_NO_OMNIPRESENT) {
		if (view->workspace == workspace) {
			return true;
		}
	}
	return false;
}

static struct workspace *
get_adjacent_occupied(struct workspace *current, struct wl_list *workspaces,
		bool wrap, bool reverse)
{
	struct server *server = current->server;
	struct wl_list *start = &current->link;
	struct wl_list *link = reverse ? start->prev : start->next;
	bool has_wrapped = false;

	while (true) {
		/* Handle list boundaries */
		if (link == workspaces) {
			if (!wrap) {
				break;  /* No wrapping allowed - stop searching */
			}
			if (has_wrapped) {
				break;  /* Already wrapped once - stop to prevent infinite loop */
			}
			/* Wrap around */
			link = reverse ? workspaces->prev : workspaces->next;
			has_wrapped = true;
			continue;
		}

		/* Get the workspace */
		struct workspace *target = wl_container_of(link, target, link);

		/* Check if we've come full circle */
		if (link == start) {
			break;
		}

		/* Check if it's occupied (and not current) */
		if (target != current && workspace_has_views(target, server)) {
			return target;
		}

		/* Move to next/prev */
		link = reverse ? link->prev : link->next;
	}

	return NULL;  /* No occupied workspace found */
}

static struct workspace *
get_prev_occupied(struct workspace *current, struct wl_list *workspaces, bool wrap)
{
	return get_adjacent_occupied(current, workspaces, wrap, true);
}

static struct workspace *
get_next_occupied(struct workspace *current, struct wl_list *workspaces, bool wrap)
{
	return get_adjacent_occupied(current, workspaces, wrap, false);
}

static int
_osd_handle_timeout(void *data)
{
	struct seat *seat = data;
	workspaces_osd_hide(seat);
	/* Don't re-check */
	return 0;
}

static void
_osd_show(struct server *server)
{
	if (!rc.workspace_config.popuptime) {
		return;
	}

	_osd_update(server);
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output_is_usable(output) && output->workspace_osd) {
			wlr_scene_node_set_enabled(&output->workspace_osd->node, true);
		}
	}
	if (keyboard_get_all_modifiers(&server->seat)) {
		/* Hidden by release of all modifiers */
		server->seat.workspace_osd_shown_by_modifier = true;
	} else {
		/* Hidden by timer */
		if (!server->seat.workspace_osd_timer) {
			server->seat.workspace_osd_timer = wl_event_loop_add_timer(
				server->wl_event_loop, _osd_handle_timeout, &server->seat);
		}
		wl_event_source_timer_update(server->seat.workspace_osd_timer,
			rc.workspace_config.popuptime);
	}
}

/* Public API */
void
workspaces_init(struct server *server)
{
	server->workspaces.cosmic_manager = lab_cosmic_workspace_manager_create(
		server->wl_display, /* capabilities */ CW_CAP_WS_ACTIVATE,
		COSMIC_WORKSPACES_VERSION);

	server->workspaces.ext_manager = lab_ext_workspace_manager_create(
		server->wl_display, /* capabilities */ WS_CAP_WS_ACTIVATE,
		EXT_WORKSPACES_VERSION);

	server->workspaces.cosmic_group = lab_cosmic_workspace_group_create(
		server->workspaces.cosmic_manager);

	server->workspaces.ext_group = lab_ext_workspace_group_create(
		server->workspaces.ext_manager);

	wl_list_init(&server->workspaces.all);

	struct workspace_config *conf;
	wl_list_for_each(conf, &rc.workspace_config.workspaces, link) {
		add_workspace(server, conf->name);
	}

	/*
	 * After adding workspaces, check if there is an initial workspace
	 * selected and set that as the initial workspace.
	 */
	char *initial_name = rc.workspace_config.initial_workspace_name;
	struct workspace *initial = NULL;
	struct workspace *first = wl_container_of(
		server->workspaces.all.next, first, link);

	if (initial_name) {
		initial = workspace_find_by_name(server, initial_name);
	}
	if (!initial) {
		initial = first;
	}

	server->workspaces.current = initial;
	wlr_scene_node_set_enabled(&initial->tree->node, true);
	lab_cosmic_workspace_set_active(initial->cosmic_workspace, true);
	lab_ext_workspace_set_active(initial->ext_workspace, true);
}

/*
 * update_focus should normally be set to true. It is set to false only
 * when this function is called from desktop_focus_view(), in order to
 * avoid unnecessary extra focus changes and possible recursion.
 */
void
workspaces_switch_to(struct workspace *target, bool update_focus)
{
	assert(target);
	struct server *server = target->server;
	if (target == server->workspaces.current) {
		return;
	}

	/* Disable the old workspace */
	wlr_scene_node_set_enabled(
		&server->workspaces.current->tree->node, false);

	lab_cosmic_workspace_set_active(
		server->workspaces.current->cosmic_workspace, false);
	lab_ext_workspace_set_active(
		server->workspaces.current->ext_workspace, false);

	/* Move Omnipresent views to new workspace */
	struct view *view;
	enum lab_view_criteria criteria =
		LAB_VIEW_CRITERIA_CURRENT_WORKSPACE;
	for_each_view_reverse(view, &server->views, criteria) {
		if (view->visible_on_all_workspaces) {
			view_move_to_workspace(view, target);
		}
	}

	/* Enable the new workspace */
	wlr_scene_node_set_enabled(&target->tree->node, true);

	/* Save the last visited workspace */
	server->workspaces.last = server->workspaces.current;

	/* Make sure new views will spawn on the new workspace */
	server->workspaces.current = target;

	struct view *grabbed_view = server->grabbed_view;
	if (grabbed_view) {
		view_move_to_workspace(grabbed_view, target);
	}

	/*
	 * Make sure we are focusing what the user sees. Only refocus if
	 * the focus is not already on an omnipresent view.
	 */
	if (update_focus) {
		struct view *active_view = server->active_view;
		if (!(active_view && active_view->visible_on_all_workspaces)) {
			desktop_focus_topmost_view(server);
		}
	}

	/* And finally show the OSD */
	_osd_show(server);

	/*
	 * Make sure we are not carrying around a
	 * cursor image from the previous desktop
	 */
	cursor_update_focus(server);

	/* Ensure that only currently visible fullscreen windows hide the top layer */
	desktop_update_top_layer_visibility(server);

	lab_cosmic_workspace_set_active(target->cosmic_workspace, true);
	lab_ext_workspace_set_active(target->ext_workspace, true);
}

void
workspaces_osd_hide(struct seat *seat)
{
	assert(seat);
	struct output *output;
	struct server *server = seat->server;
	wl_list_for_each(output, &server->outputs, link) {
		if (!output->workspace_osd) {
			continue;
		}
		wlr_scene_node_set_enabled(&output->workspace_osd->node, false);
		wlr_scene_buffer_set_buffer(output->workspace_osd, NULL);
	}
	seat->workspace_osd_shown_by_modifier = false;

	/* Update the cursor focus in case it was on top of the OSD before */
	cursor_update_focus(server);
}

struct workspace *
workspaces_find(struct workspace *anchor, const char *name, bool wrap)
{
	assert(anchor);
	if (!name) {
		return NULL;
	}
	struct server *server = anchor->server;
	struct wl_list *workspaces = &server->workspaces.all;

	if (!strcasecmp(name, "current")) {
		return anchor;
	} else if (!strcasecmp(name, "last")) {
		return server->workspaces.last;
	} else if (!strcasecmp(name, "left")) {
		return get_prev(anchor, workspaces, wrap);
	} else if (!strcasecmp(name, "right")) {
		return get_next(anchor, workspaces, wrap);
	} else if (!strcasecmp(name, "left-occupied")) {
		return get_prev_occupied(anchor, workspaces, wrap);
	} else if (!strcasecmp(name, "right-occupied")) {
		return get_next_occupied(anchor, workspaces, wrap);
	}
	return workspace_find_by_name(server, name);
}

static void
destroy_workspace(struct workspace *workspace)
{
	wlr_scene_node_destroy(&workspace->tree->node);
	zfree(workspace->name);
	wl_list_remove(&workspace->link);
	wl_list_remove(&workspace->on_cosmic.activate.link);
	wl_list_remove(&workspace->on_ext.activate.link);

	lab_cosmic_workspace_destroy(workspace->cosmic_workspace);
	lab_ext_workspace_destroy(workspace->ext_workspace);
	free(workspace);
}

void
workspaces_reconfigure(struct server *server)
{
	/*
	 * Compare actual workspace list with the new desired configuration to:
	 *   - Update names
	 *   - Add workspaces if more workspaces are desired
	 *   - Destroy workspaces if fewer workspace are desired
	 */

	struct wl_list *workspace_link = server->workspaces.all.next;

	struct workspace_config *conf;
	wl_list_for_each(conf, &rc.workspace_config.workspaces, link) {
		struct workspace *workspace = wl_container_of(
			workspace_link, workspace, link);

		if (workspace_link == &server->workspaces.all) {
			/* # of configured workspaces increased */
			wlr_log(WLR_DEBUG, "Adding workspace \"%s\"",
				conf->name);
			add_workspace(server, conf->name);
			continue;
		}
		if (strcmp(workspace->name, conf->name)) {
			/* Workspace is renamed */
			wlr_log(WLR_DEBUG, "Renaming workspace \"%s\" to \"%s\"",
				workspace->name, conf->name);
			xstrdup_replace(workspace->name, conf->name);
			lab_cosmic_workspace_set_name(
				workspace->cosmic_workspace, workspace->name);
			lab_ext_workspace_set_name(
				workspace->ext_workspace, workspace->name);
		}
		workspace_link = workspace_link->next;
	}

	if (workspace_link == &server->workspaces.all) {
		return;
	}

	/* # of configured workspaces decreased */
	overlay_finish(&server->seat);
	struct workspace *first_workspace =
		wl_container_of(server->workspaces.all.next, first_workspace, link);

	while (workspace_link != &server->workspaces.all) {
		struct workspace *workspace = wl_container_of(
			workspace_link, workspace, link);

		wlr_log(WLR_DEBUG, "Destroying workspace \"%s\"",
			workspace->name);

		struct view *view;
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace == workspace) {
				view_move_to_workspace(view, first_workspace);
			}
		}

		if (server->workspaces.current == workspace) {
			workspaces_switch_to(first_workspace,
				/* update_focus */ true);
		}
		if (server->workspaces.last == workspace) {
			server->workspaces.last = first_workspace;
		}

		workspace_link = workspace_link->next;
		destroy_workspace(workspace);
	}
}

void
workspaces_destroy(struct server *server)
{
	struct workspace *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &server->workspaces.all, link) {
		destroy_workspace(workspace);
	}
	assert(wl_list_empty(&server->workspaces.all));
}
