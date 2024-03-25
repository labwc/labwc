// SPDX-License-Identifier: GPL-2.0-only

/*
 * Helpers for view server side decorations
 *
 * Copyright (C) Johan Malm 2020-2021
 */

#include <assert.h>
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "labwc.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"
#include "window-rules.h"

struct border
ssd_thickness(struct view *view)
{
	assert(view);
	/*
	 * Check preconditions for displaying SSD. Note that this
	 * needs to work even before ssd_create() has been called.
	 *
	 * For that reason we are not using the .enabled state of
	 * the titlebar node here but rather check for the view
	 * boolean. If we were to use the .enabled state this would
	 * cause issues on Reconfigure events with views which were
	 * in border-only deco mode as view->ssd would only be set
	 * after ssd_create() returns.
	 */
	if (!view->ssd_enabled || view->fullscreen) {
		return (struct border){ 0 };
	}

	struct theme *theme = view->server->theme;

	if (view->maximized == VIEW_AXIS_BOTH) {
		struct border thickness = { 0 };
		if (!view->ssd_titlebar_hidden) {
			thickness.top += theme->title_height;
		}
		return thickness;
	}

	struct border thickness = {
		.top = theme->title_height + theme->border_width,
		.bottom = theme->border_width,
		.left = theme->border_width,
		.right = theme->border_width,
	};

	if (view->ssd_titlebar_hidden) {
		thickness.top -= theme->title_height;
	}
	return thickness;
}

struct wlr_box
ssd_max_extents(struct view *view)
{
	assert(view);
	struct border border = ssd_thickness(view);

	int eff_width = view->current.width;
	int eff_height = view_effective_height(view, /* use_pending */ false);

	return (struct wlr_box){
		.x = view->current.x - border.left,
		.y = view->current.y - border.top,
		.width = eff_width + border.left + border.right,
		.height = eff_height + border.top + border.bottom,
	};
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
ssd_get_part_type(const struct ssd *ssd, struct wlr_scene_node *node)
{
	if (!node) {
		return LAB_SSD_NONE;
	} else if (node->type == WLR_SCENE_NODE_BUFFER
			&& lab_wlr_surface_from_node(node)) {
		return LAB_SSD_CLIENT;
	} else if (!ssd) {
		return LAB_SSD_NONE;
	}

	const struct wl_list *part_list = NULL;
	struct wlr_scene_tree *grandparent =
		node->parent ? node->parent->node.parent : NULL;
	struct wlr_scene_tree *greatgrandparent =
		grandparent ? grandparent->node.parent : NULL;

	/* active titlebar */
	if (node->parent == ssd->titlebar.active.tree) {
		part_list = &ssd->titlebar.active.parts;
	} else if (grandparent == ssd->titlebar.active.tree) {
		part_list = &ssd->titlebar.active.parts;
	} else if (greatgrandparent == ssd->titlebar.active.tree) {
		part_list = &ssd->titlebar.active.parts;

	/* extents */
	} else if (node->parent == ssd->extents.tree) {
		part_list = &ssd->extents.parts;

	/* active border */
	} else if (node->parent == ssd->border.active.tree) {
		part_list = &ssd->border.active.parts;

	/* inactive titlebar */
	} else if (node->parent == ssd->titlebar.inactive.tree) {
		part_list = &ssd->titlebar.inactive.parts;
	} else if (grandparent == ssd->titlebar.inactive.tree) {
		part_list = &ssd->titlebar.inactive.parts;
	} else if (greatgrandparent == ssd->titlebar.inactive.tree) {
		part_list = &ssd->titlebar.inactive.parts;

	/* inactive border */
	} else if (node->parent == ssd->border.inactive.tree) {
		part_list = &ssd->border.inactive.parts;
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
ssd_at(const struct ssd *ssd, struct wlr_scene *scene, double lx, double ly)
{
	assert(scene);
	double sx, sy;
	struct wlr_scene_node *node = wlr_scene_node_at(
		&scene->tree.node, lx, ly, &sx, &sy);
	return ssd_get_part_type(ssd, node);
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
		return WLR_EDGE_NONE;
	}
}

struct ssd *
ssd_create(struct view *view, bool active)
{
	assert(view);
	struct ssd *ssd = znew(*ssd);

	ssd->view = view;
	ssd->tree = wlr_scene_tree_create(view->scene_tree);
	wlr_scene_node_lower_to_bottom(&ssd->tree->node);
	ssd->titlebar.height = view->server->theme->title_height;
	ssd_extents_create(ssd);
	ssd_border_create(ssd);
	ssd_titlebar_create(ssd);
	if (view->ssd_titlebar_hidden) {
		/* Ensure we keep the old state on Reconfigure or when exiting fullscreen */
		ssd_titlebar_hide(ssd);
	}
	ssd->margin = ssd_thickness(view);
	ssd_set_active(ssd, active);
	ssd_enable_keybind_inhibit_indicator(ssd, view->inhibits_keybinds);
	ssd->state.geometry = view->current;

	return ssd;
}

struct border
ssd_get_margin(const struct ssd *ssd)
{
	return ssd ? ssd->margin : (struct border){ 0 };
}

void
ssd_update_margin(struct ssd *ssd)
{
	if (!ssd) {
		return;
	}
	ssd->margin = ssd_thickness(ssd->view);
}

void
ssd_update_geometry(struct ssd *ssd)
{
	if (!ssd) {
		return;
	}

	struct wlr_box cached = ssd->state.geometry;
	struct wlr_box current = ssd->view->current;

	int eff_width = current.width;
	int eff_height = view_effective_height(ssd->view, /* use_pending */ false);

	if (eff_width == cached.width && eff_height == cached.height) {
		if (current.x != cached.x || current.y != cached.y) {
			/* Dynamically resize extents based on position and usable_area */
			ssd_extents_update(ssd);
			ssd->state.geometry = current;
		}
		bool maximized = (ssd->view->maximized == VIEW_AXIS_BOTH);
		if (ssd->state.was_maximized != maximized) {
			ssd_border_update(ssd);
			ssd_titlebar_update(ssd);
			/*
			 * Not strictly necessary as ssd_titlebar_update()
			 * already sets state.was_maximized but to future
			 * proof this a bit we also set it here again.
			 */
			ssd->state.was_maximized = maximized;
		}
		return;
	}
	ssd_extents_update(ssd);
	ssd_border_update(ssd);
	ssd_titlebar_update(ssd);
	ssd->state.geometry = current;
}

void
ssd_titlebar_hide(struct ssd *ssd)
{
	if (!ssd || !ssd->titlebar.tree->node.enabled) {
		return;
	}
	wlr_scene_node_set_enabled(&ssd->titlebar.tree->node, false);
	ssd->titlebar.height = 0;
	ssd_border_update(ssd);
	ssd_extents_update(ssd);
	ssd->margin = ssd_thickness(ssd->view);
}

void
ssd_destroy(struct ssd *ssd)
{
	if (!ssd) {
		return;
	}

	/* Maybe reset hover view */
	struct view *view = ssd->view;
	struct ssd_hover_state *hover_state;
	hover_state = view->server->ssd_hover_state;
	if (hover_state->view == view) {
		hover_state->view = NULL;
		hover_state->button = NULL;
	}

	/* Destroy subcomponents */
	ssd_titlebar_destroy(ssd);
	ssd_border_destroy(ssd);
	ssd_extents_destroy(ssd);
	wlr_scene_node_destroy(&ssd->tree->node);

	free(ssd);
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
	if (whole == LAB_SSD_PART_TITLE) {
		/* "Title" includes blank areas of "Titlebar" as well */
		return candidate >= LAB_SSD_PART_TITLEBAR
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
ssd_set_active(struct ssd *ssd, bool active)
{
	if (!ssd) {
		return;
	}
	wlr_scene_node_set_enabled(&ssd->border.active.tree->node, active);
	wlr_scene_node_set_enabled(&ssd->titlebar.active.tree->node, active);
	wlr_scene_node_set_enabled(&ssd->border.inactive.tree->node, !active);
	wlr_scene_node_set_enabled(&ssd->titlebar.inactive.tree->node, !active);
}

void
ssd_enable_shade(struct ssd *ssd, bool enable)
{
	if (!ssd) {
		return;
	}
	ssd_border_update(ssd);
	wlr_scene_node_set_enabled(&ssd->extents.tree->node, !enable);
}

void
ssd_enable_keybind_inhibit_indicator(struct ssd *ssd, bool enable)
{
	if (!ssd) {
		return;
	}

	float *custom_color = window_rules_get_custom_border_color(ssd->view);

	float *color = custom_color ? custom_color :
		(enable ? rc.theme->window_toggled_keybinds_color
				: rc.theme->window_active_border_color);

	struct ssd_part *part = ssd_get_part(&ssd->border.active.parts, LAB_SSD_PART_TOP);
	struct wlr_scene_rect *rect = wlr_scene_rect_from_node(part->node);
	wlr_scene_rect_set_color(rect, color);
}

struct ssd_hover_state *
ssd_hover_state_new(void)
{
	return znew(struct ssd_hover_state);
}

enum ssd_part_type
ssd_button_get_type(const struct ssd_button *button)
{
	return button ? button->type : LAB_SSD_NONE;
}

struct view *
ssd_button_get_view(const struct ssd_button *button)
{
	return button ? button->view : NULL;
}

bool
ssd_debug_is_root_node(const struct ssd *ssd, struct wlr_scene_node *node)
{
	if (!ssd || !node) {
		return false;
	}
	return node == &ssd->tree->node;
}

const char *
ssd_debug_get_node_name(const struct ssd *ssd, struct wlr_scene_node *node)
{
	if (!ssd || !node) {
		return NULL;
	}
	if (node == &ssd->tree->node) {
		return "view->ssd";
	}
	if (node == &ssd->titlebar.active.tree->node) {
		return "titlebar.active";
	}
	if (node == &ssd->titlebar.inactive.tree->node) {
		return "titlebar.inactive";
	}
	if (node == &ssd->border.active.tree->node) {
		return "border.active";
	}
	if (node == &ssd->border.inactive.tree->node) {
		return "border.inactive";
	}
	if (node == &ssd->extents.tree->node) {
		return "extents";
	}
	return NULL;
}
