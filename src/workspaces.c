// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "labwc.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/zfree.h"
#include "workspaces.h"

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
	uint16_t rect_height = 20;
	uint16_t rect_width = 20;

	/* Dimensions */
	size_t workspace_count = wl_list_length(&server->workspaces);
	uint16_t marker_width = workspace_count * (rect_width + padding) - padding;
	uint16_t width = margin * 2 + (marker_width < 200 ? 200 : marker_width);
	uint16_t height = margin * 3 + rect_height + font_height(&rc.font_osd);

	cairo_t *cairo;
	cairo_surface_t *surface;
	struct workspace *workspace;

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct lab_data_buffer *buffer = buffer_create_cairo(width, height,
			output->wlr_output->scale, true);
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
		draw_cairo_border(cairo, width, height, theme->osd_border_width);

		uint16_t x = (width - marker_width) / 2;
		wl_list_for_each(workspace, &server->workspaces, link) {
			bool active =  workspace == server->workspace_current;
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

		/* Text */
		set_cairo_color(cairo, server->theme->osd_label_text_color);
		PangoLayout *layout = pango_cairo_create_layout(cairo);
		pango_layout_set_width(layout, (width - 2 * margin) * PANGO_SCALE);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

		PangoFontDescription *desc = font_to_pango_desc(&rc.font_osd);
		pango_layout_set_font_description(layout, desc);

		/* Center workspace indicator on the x axis */
		x = font_width(&rc.font_osd, server->workspace_current->name);
		x = (width - x) / 2;
		cairo_move_to(cairo, x, margin * 2 + rect_height);
		//pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);
		pango_layout_set_text(layout, server->workspace_current->name, -1);
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
			+ (output->usable_area.height - height ) / 2
			+ output_box.y;
		wlr_scene_node_set_position(&output->workspace_osd->node, lx, ly);
		wlr_scene_buffer_set_buffer(output->workspace_osd, &buffer->base);
		wlr_scene_buffer_set_dest_size(output->workspace_osd,
			buffer->unscaled_width, buffer->unscaled_height);

		/* And finally drop the buffer so it will get destroyed on OSD hide */
		wlr_buffer_drop(&buffer->base);
	}
}

/* Internal API */
static void
add_workspace(struct server *server, const char *name)
{
	struct workspace *workspace = calloc(1, sizeof(struct workspace));
	workspace->server = server;
	workspace->name = strdup(name);
	workspace->tree = wlr_scene_tree_create(server->view_tree);
	wl_list_insert(server->workspaces.prev, &workspace->link);
	if (!server->workspace_current) {
		server->workspace_current = workspace;
	} else {
		wlr_scene_node_set_enabled(&workspace->tree->node, false);
	}
}

static struct workspace *
get_prev(struct workspace *current, struct wl_list *workspaces)
{
	struct wl_list *target_link = current->link.prev;
	if (target_link == workspaces) {
		/* Current workspace is the first one, roll over */
		target_link = target_link->prev;
	}
	return wl_container_of(target_link, current, link);
}

static struct workspace *
get_next(struct workspace *current, struct wl_list *workspaces)
{
	struct wl_list *target_link = current->link.next;
	if (target_link == workspaces) {
		/* Current workspace is the last one, roll over */
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
		wlr_scene_node_set_enabled(&output->workspace_osd->node, true);
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
	wl_list_init(&server->workspaces);

	struct workspace *conf;
	wl_list_for_each(conf, &rc.workspace_config.workspaces, link) {
		add_workspace(server, conf->name);
	}
}

void
workspaces_switch_to(struct workspace *target)
{
	assert(target);
	if (target == target->server->workspace_current) {
		return;
	}

	/* Disable the old workspace */
	wlr_scene_node_set_enabled(
		&target->server->workspace_current->tree->node, false);

	/* Enable the new workspace */
	wlr_scene_node_set_enabled(&target->tree->node, true);

	/* Save the last visited workspace */
	target->server->workspace_last = target->server->workspace_current;

	/* Make sure new views will spawn on the new workspace */
	target->server->workspace_current = target;

	/**
	 * Make sure we are focusing what the user sees.
	 *
	 * TODO: This is an issue for always-on-top views as they will
	 * loose keyboard focus once switching to another workspace.
	 */
	desktop_focus_topmost_mapped_view(target->server);

	/* And finally show the OSD */
	_osd_show(target->server);
}

void
workspaces_send_to(struct view *view, struct workspace *target)
{
	assert(view);
	assert(target);
	if (view->workspace == target) {
		return;
	}
	wlr_scene_node_reparent(&view->scene_tree->node, target->tree);
	view->workspace = target;
}


void
workspaces_osd_hide(struct seat *seat)
{
	assert(seat);
	struct output *output;
	struct server *server = seat->server;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_scene_node_set_enabled(&output->workspace_osd->node, false);
		wlr_scene_buffer_set_buffer(output->workspace_osd, NULL);
	}
	seat->workspace_osd_shown_by_modifier = false;
}

struct workspace *
workspaces_find(struct workspace *anchor, const char *name)
{
	assert(anchor);
	if (!name) {
		return NULL;
	}
	size_t index = 0;
	struct workspace *target;
	size_t wants_index = parse_workspace_index(name);
	struct wl_list *workspaces = &anchor->server->workspaces;

	if (wants_index) {
		wl_list_for_each(target, workspaces, link) {
			if (wants_index == ++index) {
				return target;
			}
		}
	} else if (!strcasecmp(name, "last")) {
		return anchor->server->workspace_last;
	} else if (!strcasecmp(name, "left")) {
		return get_prev(anchor, workspaces);
	} else if (!strcasecmp(name, "right")) {
		return get_next(anchor, workspaces);
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

void
workspaces_destroy(struct server *server)
{
	struct workspace *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &server->workspaces, link) {
		wlr_scene_node_destroy(&workspace->tree->node);
		zfree(workspace->name);
		wl_list_remove(&workspace->link);
		free(workspace);
	}
	assert(wl_list_empty(&server->workspaces));
}
