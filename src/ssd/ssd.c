// SPDX-License-Identifier: GPL-2.0-only

/*
 * Helpers for view server side decorations
 *
 * Copyright (C) Johan Malm 2020-2021
 */

#include <assert.h>
#include "config/rcxml.h"
#include "common/font.h"
#include "labwc.h"
#include "theme.h"
#include "ssd.h"

/* TODO: use theme->title_height instead of SSD_HEIGHT */
struct border
ssd_thickness(struct view *view)
{
	struct theme *theme = view->server->theme;
	struct border border = {
		.top = SSD_HEIGHT,
		.bottom = theme->border_width,
		.left = theme->border_width,
		.right = theme->border_width,
	};
	return border;
}

struct wlr_box
ssd_max_extents(struct view *view)
{
	struct border border = ssd_thickness(view);
	struct wlr_box box = {
		.x = view->x - border.left,
		.y = view->y - border.top,
		.width = view->w + border.left + border.right,
		.height = view->h + border.top + border.bottom,
	};
	return box;
}

bool
ssd_is_button(enum ssd_part_type type)
{
	return type == LAB_SSD_BUTTON_CLOSE
		|| type == LAB_SSD_BUTTON_MAXIMIZE
		|| type == LAB_SSD_BUTTON_ICONIFY
		|| type == LAB_SSD_BUTTON_WINDOW_MENU;
}

enum ssd_part_type
ssd_get_part_type(struct view *view, struct wlr_scene_node *node)
{
	if (!node) {
		return LAB_SSD_NONE;
	} else if (node->type == WLR_SCENE_NODE_SURFACE) {
		return LAB_SSD_CLIENT;
	} else if (!view->ssd.tree) {
		return LAB_SSD_NONE;
	}

	struct wl_list *part_list = NULL;
	struct wlr_scene_node *grandparent =
		node->parent ? node->parent->parent : NULL;

	/* active titlebar */
	if (node->parent == &view->ssd.titlebar.active.tree->node) {
		part_list = &view->ssd.titlebar.active.parts;
	} else if (grandparent == &view->ssd.titlebar.active.tree->node) {
		part_list = &view->ssd.titlebar.active.parts;

	/* extents */
	} else if (node->parent == &view->ssd.extents.tree->node) {
		part_list = &view->ssd.extents.parts;

	/* active border */
	} else if (node->parent == &view->ssd.border.active.tree->node) {
		part_list = &view->ssd.border.active.parts;

	/* inactive titlebar */
	} else if (node->parent == &view->ssd.titlebar.inactive.tree->node) {
		part_list = &view->ssd.titlebar.inactive.parts;
	} else if (grandparent == &view->ssd.titlebar.inactive.tree->node) {
		part_list = &view->ssd.titlebar.inactive.parts;

	/* inactive border */
	} else if (node->parent == &view->ssd.border.inactive.tree->node) {
		part_list = &view->ssd.border.inactive.parts;
	}

	if (part_list) {
		struct ssd_part *part;
		wl_list_for_each(part, part_list, link) {
			if (node == part->node) {
				return part->type;
			}
		}
	}
	return LAB_SSD_NONE;
}

enum ssd_part_type
ssd_at(struct view *view, double lx, double ly)
{
	double sx, sy;
	struct wlr_scene_node *node = wlr_scene_node_at(
		&view->server->scene->node, lx, ly, &sx, &sy);
	return ssd_get_part_type(view, node);
}

uint32_t
ssd_resize_edges(enum ssd_part_type type)
{
	switch (type) {
	case LAB_SSD_PART_TOP:
		return WLR_EDGE_TOP;
	case LAB_SSD_PART_RIGHT:
		return WLR_EDGE_RIGHT;
	case LAB_SSD_PART_BOTTOM:
		return WLR_EDGE_BOTTOM;
	case LAB_SSD_PART_LEFT:
		return WLR_EDGE_LEFT;
	case LAB_SSD_PART_CORNER_TOP_LEFT:
		return WLR_EDGE_TOP | WLR_EDGE_LEFT;
	case LAB_SSD_PART_CORNER_TOP_RIGHT:
		return WLR_EDGE_RIGHT | WLR_EDGE_TOP;
	case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
		return WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;
	case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
		return WLR_EDGE_BOTTOM | WLR_EDGE_LEFT;
	default:
		break;
	}
	return 0;
}

static void
_ssd_set_active(struct ssd *ssd, bool active)
{
	wlr_scene_node_set_enabled(&ssd->border.active.tree->node, active);
	wlr_scene_node_set_enabled(&ssd->titlebar.active.tree->node, active);
	wlr_scene_node_set_enabled(&ssd->border.inactive.tree->node, !active);
	wlr_scene_node_set_enabled(&ssd->titlebar.inactive.tree->node, !active);
}

void
ssd_create(struct view *view)
{
	if (view->ssd.tree) {
		/* SSD was hidden. Just enable it */
		wlr_scene_node_set_enabled(&view->ssd.tree->node, true);
		return;
	}

	view->ssd.tree = wlr_scene_tree_create(&view->scene_tree->node);
	wlr_scene_node_lower_to_bottom(&view->ssd.tree->node);
	ssd_extents_create(view);
	ssd_titlebar_create(view);
	ssd_border_create(view);
}

void
ssd_update_geometry(struct view *view)
{
	/* TODO: verify we are not called without reason. like in commit handlers */
	if (!view->ssd.tree || !view->scene_node) {
		return;
	}

	if (view->ssd.enabled && !view->ssd.tree->node.state.enabled) {
		wlr_scene_node_set_enabled(&view->ssd.tree->node, true);
	}
	if (!view->ssd.enabled && view->ssd.tree->node.state.enabled) {
		wlr_scene_node_set_enabled(&view->ssd.tree->node, false);
	}

	int width = view->w;
	int height = view->h;
	if (width == view->ssd.state.width && height == view->ssd.state.height) {
		return;
	}
	ssd_extents_update(view);
	ssd_border_update(view);
	ssd_titlebar_update(view);

	view->ssd.state.width = width;
	view->ssd.state.height = height;
}

void
ssd_hide(struct view *view)
{
	if (!view->ssd.tree) {
		return;
	}

	wlr_log(WLR_ERROR, "Hiding SSD");
	wlr_scene_node_set_enabled(&view->ssd.tree->node, false);
}

void ssd_reload(struct view *view)
{
	if (!view->ssd.tree) {
		return;
	}

	bool view_was_active = view->server->ssd_focused_view == view;
	ssd_destroy(view);
	ssd_create(view);
	if (view_was_active) {
		view->server->ssd_focused_view = view;
	} else {
		_ssd_set_active(&view->ssd, false);
	}
}

void
ssd_destroy(struct view *view)
{
	if (!view->ssd.tree) {
		return;
	}

	/* Maybe reset focused view */
	if (view->server->ssd_focused_view == view) {
		view->server->ssd_focused_view = NULL;
	}

	/* Maybe reset hover view */
	struct ssd_hover_state *hover_state;
	hover_state = &view->server->ssd_hover_state;
	if (hover_state->view == view) {
		hover_state->view = NULL;
		hover_state->type = LAB_SSD_NONE;
		hover_state->node = NULL;
	}

	/* Destroy subcomponents */
	ssd_titlebar_destroy(view);
	ssd_border_destroy(view);
	ssd_extents_destroy(view);
	wlr_scene_node_destroy(&view->ssd.tree->node);
	view->ssd.tree = NULL;
}

bool
ssd_part_contains(enum ssd_part_type whole, enum ssd_part_type candidate)
{
	if (whole == candidate) {
		return true;
	}
	if (whole == LAB_SSD_PART_TITLEBAR) {
		return candidate >= LAB_SSD_BUTTON_CLOSE
			&& candidate <= LAB_SSD_PART_TITLE;
	}
	if (whole == LAB_SSD_FRAME) {
		return candidate >= LAB_SSD_BUTTON_CLOSE
			&& candidate <= LAB_SSD_CLIENT;
	}
	if (whole == LAB_SSD_PART_TOP) {
		return candidate == LAB_SSD_PART_CORNER_TOP_LEFT
			|| candidate == LAB_SSD_PART_CORNER_BOTTOM_LEFT;
	}
	if (whole == LAB_SSD_PART_RIGHT) {
		return candidate == LAB_SSD_PART_CORNER_TOP_RIGHT
			|| candidate == LAB_SSD_PART_CORNER_BOTTOM_RIGHT;
	}
	if (whole == LAB_SSD_PART_BOTTOM) {
		return candidate == LAB_SSD_PART_CORNER_BOTTOM_RIGHT
			|| candidate == LAB_SSD_PART_CORNER_BOTTOM_LEFT;
	}
	if (whole == LAB_SSD_PART_LEFT) {
		return candidate == LAB_SSD_PART_CORNER_TOP_LEFT
			|| candidate == LAB_SSD_PART_CORNER_BOTTOM_LEFT;
	}
	return false;
}

void
ssd_set_active(struct view *view)
{
	if (!view->ssd.tree) {
		return;
	}

	struct view *last = view->server->ssd_focused_view;
	if (last == view) {
		return;
	}
	if (last && last->ssd.tree) {
		_ssd_set_active(&last->ssd, false);
	}
	_ssd_set_active(&view->ssd, true);
	view->server->ssd_focused_view = view;
}
