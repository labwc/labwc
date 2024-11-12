// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "config.h"
#include "buffer.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/list.h"
#include "common/mem.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "protocols/cosmic-workspaces.h"
#include "view.h"
#include "workspaces.h"
#include "xwayland.h"

#define COSMIC_WORKSPACES_VERSION 1

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

		cairo = buffer->cairo;

		/* Background */
		set_cairo_color(cairo, theme->osd_bg_color);
		cairo_rectangle(cairo, 0, 0, width, height);
		cairo_fill(cairo);

		/* Border */
		set_cairo_color(cairo, theme->osd_border_color);
		struct wlr_fbox fbox = {
			.width = width,
			.height = height,
		};
		draw_cairo_border(cairo, fbox, theme->osd_border_width);

		/* Boxes */
		uint16_t x;
		if (!hide_boxes) {
			x = (width - marker_width) / 2;
			wl_list_for_each(workspace, &server->workspaces.all, link) {
				bool active =  workspace == server->workspaces.current;
				set_cairo_color(cairo, server->theme->osd_label_text_color);
				cairo_rectangle(cairo, x, margin,
					rect_width - padding, rect_height);
				cairo_stroke(cairo);
				if (active) {
					cairo_rectangle(cairo, x, margin,
						rect_width - padding, rect_height);
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

		if (!output->workspace_osd) {
			output->workspace_osd = wlr_scene_buffer_create(
				&server->scene->tree, NULL);
		}
		/* Position the whole thing */
		struct wlr_box output_box;
		wlr_output_layout_get_box(output->server->output_layout,
			output->wlr_output, &output_box);
		int lx = output->usable_area.x
			+ (output->usable_area.width - width) / 2
			+ output_box.x;
		int ly = output->usable_area.y
			+ (output->usable_area.height - height) / 2
			+ output_box.y;
		wlr_scene_node_set_position(&output->workspace_osd->node, lx, ly);
		wlr_scene_buffer_set_buffer(output->workspace_osd, &buffer->base);
		wlr_scene_buffer_set_dest_size(output->workspace_osd,
			buffer->logical_width, buffer->logical_height);

		/* And finally drop the buffer so it will get destroyed on OSD hide */
		wlr_buffer_drop(&buffer->base);
	}
}

/* cosmic workspace handlers */
static void
handle_workspace_activate(struct wl_listener *listener, void *data)
{
	struct workspace *workspace = wl_container_of(listener, workspace, on.activate);
	workspaces_switch_to(workspace, /* update_focus */ true);
	wlr_log(WLR_INFO, "activating workspace %s", workspace->name);
}

/* Internal API */
static void
add_workspace(struct server *server, const char *name)
{
	struct workspace *workspace = znew(*workspace);
	workspace->server = server;
	workspace->name = xstrdup(name);
	workspace->tree = wlr_scene_tree_create(server->view_tree);
	wl_list_append(&server->workspaces.all, &workspace->link);
	if (!server->workspaces.current) {
		server->workspaces.current = workspace;
	} else {
		wlr_scene_node_set_enabled(&workspace->tree->node, false);
	}

	bool active = server->workspaces.current == workspace;
	workspace->cosmic_workspace = lab_cosmic_workspace_create(server->workspaces.cosmic_group);
	lab_cosmic_workspace_set_name(workspace->cosmic_workspace, name);
	lab_cosmic_workspace_set_active(workspace->cosmic_workspace, active);
	lab_cosmic_workspace_set_hidden(workspace->cosmic_workspace, !active);

	workspace->on.activate.notify = handle_workspace_activate;
	wl_signal_add(&workspace->cosmic_workspace->events.activate, &workspace->on.activate);
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
	struct wlr_keyboard *keyboard = &server->seat.keyboard_group->keyboard;
	if (keyboard_any_modifiers_pressed(keyboard)) {
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

	server->workspaces.cosmic_group = lab_cosmic_workspace_group_create(
		server->workspaces.cosmic_manager);

	wl_list_init(&server->workspaces.all);

	struct workspace *conf;
	wl_list_for_each(conf, &rc.workspace_config.workspaces, link) {
		add_workspace(server, conf->name);
	}
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
	lab_cosmic_workspace_set_hidden(
		server->workspaces.current->cosmic_workspace, true);

	/* Move Omnipresent views to new workspace */
	struct view *view;
	enum lab_view_criteria criteria =
		LAB_VIEW_CRITERIA_CURRENT_WORKSPACE;
	for_each_view(view, &server->views, criteria) {
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

	/*
	 * Make sure we are focusing what the user sees. Only refocus if
	 * the focus is not already on an omnipresent or always-on-top view.
	 *
	 * TODO: Decouple always-on-top views from the omnipresent state.
	 *       One option for that would be to create a new scene tree
	 *       as child of every workspace tree and then reparent a-o-t
	 *       windows to that one. Combined with adjusting the condition
	 *       below that should take care of the issue.
	 */
	if (update_focus) {
		struct view *view = server->active_view;
		if (!view || (!view->visible_on_all_workspaces
				&& !view_is_always_on_top(view))) {
			desktop_focus_topmost_view(server);
		}
	}

	/* And finally show the OSD */
	_osd_show(server);

#if HAVE_XWAYLAND
	/* Ensure xwayland internal stacking order corresponds to the current workspace  */
	xwayland_adjust_stacking_order(server);
#endif
	/*
	 * Make sure we are not carrying around a
	 * cursor image from the previous desktop
	 */
	cursor_update_focus(server);

	/* Ensure that only currently visible fullscreen windows hide the top layer */
	desktop_update_top_layer_visiblity(server);

	lab_cosmic_workspace_set_active(target->cosmic_workspace, true);
	lab_cosmic_workspace_set_hidden(target->cosmic_workspace, false);
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
	size_t index = 0;
	struct workspace *target;
	size_t wants_index = parse_workspace_index(name);
	struct wl_list *workspaces = &anchor->server->workspaces.all;

	if (wants_index) {
		wl_list_for_each(target, workspaces, link) {
			if (wants_index == ++index) {
				return target;
			}
		}
	} else if (!strcasecmp(name, "current")) {
		return anchor;
	} else if (!strcasecmp(name, "last")) {
		return anchor->server->workspaces.last;
	} else if (!strcasecmp(name, "left")) {
		return get_prev(anchor, workspaces, wrap);
	} else if (!strcasecmp(name, "right")) {
		return get_next(anchor, workspaces, wrap);
	} else {
		wl_list_for_each(target, workspaces, link) {
			if (!strcasecmp(target->name, name)) {
				return target;
			}
		}
	}
	wlr_log(WLR_ERROR, "Workspace '%s' not found", name);
	return NULL;
}

static void
destroy_workspace(struct workspace *workspace)
{
	wlr_scene_node_destroy(&workspace->tree->node);
	zfree(workspace->name);
	wl_list_remove(&workspace->link);

	lab_cosmic_workspace_destroy(workspace->cosmic_workspace);
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

	struct wl_list *actual_workspace_link = server->workspaces.all.next;

	struct workspace *configured_workspace;
	wl_list_for_each(configured_workspace,
			&rc.workspace_config.workspaces, link) {
		struct workspace *actual_workspace = wl_container_of(
			actual_workspace_link, actual_workspace, link);

		if (actual_workspace_link == &server->workspaces.all) {
			/* # of configured workspaces increased */
			wlr_log(WLR_DEBUG, "Adding workspace \"%s\"",
				configured_workspace->name);
			add_workspace(server, configured_workspace->name);
			continue;
		}
		if (strcmp(actual_workspace->name, configured_workspace->name)) {
			/* Workspace is renamed */
			wlr_log(WLR_DEBUG, "Renaming workspace \"%s\" to \"%s\"",
				actual_workspace->name, configured_workspace->name);
			free(actual_workspace->name);
			actual_workspace->name = xstrdup(configured_workspace->name);
			lab_cosmic_workspace_set_name(
				actual_workspace->cosmic_workspace, actual_workspace->name);
		}
		actual_workspace_link = actual_workspace_link->next;
	}

	if (actual_workspace_link == &server->workspaces.all) {
		return;
	}

	/* # of configured workspaces decreased */
	overlay_hide(&server->seat);
	struct workspace *first_workspace =
		wl_container_of(server->workspaces.all.next, first_workspace, link);

	while (actual_workspace_link != &server->workspaces.all) {
		struct workspace *actual_workspace = wl_container_of(
			actual_workspace_link, actual_workspace, link);

		wlr_log(WLR_DEBUG, "Destroying workspace \"%s\"",
			actual_workspace->name);

		struct view *view;
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace == actual_workspace) {
				view_move_to_workspace(view, first_workspace);
			}
		}

		if (server->workspaces.current == actual_workspace) {
			workspaces_switch_to(first_workspace,
				/* update_focus */ true);
		}
		if (server->workspaces.last == actual_workspace) {
			server->workspaces.last = first_workspace;
		}

		actual_workspace_link = actual_workspace_link->next;
		destroy_workspace(actual_workspace);
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
