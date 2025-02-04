// SPDX-License-Identifier: GPL-2.0-only

/*
 * Helpers for view server side decorations
 *
 * Copyright (C) Johan Malm 2020-2021
 */

#include <assert.h>
#include <strings.h>
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "labwc.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

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
			thickness.top += theme->titlebar_height;
		}
		return thickness;
	}

	struct border thickness = {
		.top = theme->titlebar_height + theme->border_width,
		.bottom = theme->border_width,
		.left = theme->border_width,
		.right = theme->border_width,
	};

	if (view->ssd_titlebar_hidden) {
		thickness.top -= theme->titlebar_height;
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

/*
 * Resizing and mouse contexts like 'Left', 'TLCorner', etc. in the vicinity of
 * SSD borders, titlebars and extents can have effective "corner regions" that
 * behave differently from single-edge contexts.
 *
 * Corner regions are active whenever the cursor is within a prescribed size
 * (generally rc.resize_corner_range, but clipped to view size) of the view
 * bounds, so check the cursor against the view here.
 */
static enum ssd_part_type
get_resizing_type(const struct ssd *ssd, struct wlr_cursor *cursor)
{
	struct view *view = ssd ? ssd->view : NULL;
	if (!view || !cursor || !view->ssd_enabled || view->fullscreen) {
		return LAB_SSD_NONE;
	}

	struct wlr_box view_box = view->current;
	view_box.height = view_effective_height(view, /* use_pending */ false);

	if (!view->ssd_titlebar_hidden) {
		/* If the titlebar is visible, consider it part of the view */
		int titlebar_height = view->server->theme->titlebar_height;
		view_box.y -= titlebar_height;
		view_box.height += titlebar_height;
	}

	if (wlr_box_contains_point(&view_box, cursor->x, cursor->y)) {
		/* A cursor in bounds of the view is never in an SSD context */
		return LAB_SSD_NONE;
	}

	int corner_height = MAX(0, MIN(rc.resize_corner_range, view_box.height / 2));
	int corner_width = MAX(0, MIN(rc.resize_corner_range, view_box.width / 2));
	bool left = cursor->x < view_box.x + corner_width;
	bool right = cursor->x > view_box.x + view_box.width - corner_width;
	bool top = cursor->y < view_box.y + corner_height;
	bool bottom = cursor->y > view_box.y + view_box.height - corner_height;

	if (top && left) {
		return LAB_SSD_PART_CORNER_TOP_LEFT;
	} else if (top && right) {
		return LAB_SSD_PART_CORNER_TOP_RIGHT;
	} else if (bottom && left) {
		return LAB_SSD_PART_CORNER_BOTTOM_LEFT;
	} else if (bottom && right) {
		return LAB_SSD_PART_CORNER_BOTTOM_RIGHT;
	} else if (top) {
		return LAB_SSD_PART_TOP;
	} else if (bottom) {
		return LAB_SSD_PART_BOTTOM;
	} else if (left) {
		return LAB_SSD_PART_LEFT;
	} else if (right) {
		return LAB_SSD_PART_RIGHT;
	}

	return LAB_SSD_NONE;
}

enum ssd_part_type
ssd_get_part_type(const struct ssd *ssd, struct wlr_scene_node *node,
		struct wlr_cursor *cursor)
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

	enum ssd_part_type part_type = LAB_SSD_NONE;

	if (part_list) {
		struct ssd_part *part;
		wl_list_for_each(part, part_list, link) {
			if (node == part->node) {
				part_type = part->type;
				break;
			}
		}
	}

	if (part_type == LAB_SSD_NONE) {
		return part_type;
	}

	/* Perform cursor-based context checks */
	enum ssd_part_type resizing_type = get_resizing_type(ssd, cursor);
	return resizing_type != LAB_SSD_NONE ? resizing_type : part_type;
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
	ssd->titlebar.height = view->server->theme->titlebar_height;
	ssd_shadow_create(ssd);
	ssd_extents_create(ssd);
	/*
	 * We need to create the borders after the titlebar because it sets
	 * ssd->state.squared which ssd_border_create() reacts to.
	 * TODO: Set the state here instead so the order does not matter
	 * anymore.
	 */
	ssd_titlebar_create(ssd);
	ssd_border_create(ssd);
	if (view->ssd_titlebar_hidden) {
		/* Ensure we keep the old state on Reconfigure or when exiting fullscreen */
		ssd_set_titlebar(ssd, false);
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

int
ssd_get_corner_width(void)
{
	/* ensure a minimum corner width */
	return MAX(rc.corner_radius, 5);
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

	struct view *view = ssd->view;
	assert(view);

	struct wlr_box cached = ssd->state.geometry;
	struct wlr_box current = view->current;

	int eff_width = current.width;
	int eff_height = view_effective_height(view, /* use_pending */ false);

	bool update_area = eff_width != cached.width || eff_height != cached.height;
	bool update_extents = update_area
		|| current.x != cached.x || current.y != cached.y;

	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	bool squared = ssd_should_be_squared(ssd);

	bool state_changed = ssd->state.was_maximized != maximized
		|| ssd->state.was_shaded != view->shaded
		|| ssd->state.was_squared != squared
		|| ssd->state.was_omnipresent != view->visible_on_all_workspaces;

	if (update_extents) {
		ssd_extents_update(ssd);
	}

	if (update_area || state_changed) {
		ssd_titlebar_update(ssd);
		ssd_border_update(ssd);
		ssd_shadow_update(ssd);
	}

	if (update_extents) {
		ssd->state.geometry = current;
	}
}

void
ssd_set_titlebar(struct ssd *ssd, bool enabled)
{
	if (!ssd || ssd->titlebar.tree->node.enabled == enabled) {
		return;
	}
	wlr_scene_node_set_enabled(&ssd->titlebar.tree->node, enabled);
	ssd->titlebar.height = enabled ? ssd->view->server->theme->titlebar_height : 0;
	ssd_border_update(ssd);
	ssd_extents_update(ssd);
	ssd_shadow_update(ssd);
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
	ssd_shadow_destroy(ssd);
	wlr_scene_node_destroy(&ssd->tree->node);

	free(ssd);
}

bool
ssd_part_contains(enum ssd_part_type whole, enum ssd_part_type candidate)
{
	if (whole == candidate || whole == LAB_SSD_ALL) {
		return true;
	}
	if (whole == LAB_SSD_BUTTON) {
		return candidate >= LAB_SSD_BUTTON_CLOSE
			&& candidate <= LAB_SSD_BUTTON_OMNIPRESENT;
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
			|| candidate == LAB_SSD_PART_CORNER_TOP_RIGHT;
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

enum ssd_mode
ssd_mode_parse(const char *mode)
{
	if (!mode) {
		return LAB_SSD_MODE_INVALID;
	}
	if (!strcasecmp(mode, "none")) {
		return LAB_SSD_MODE_NONE;
	} else if (!strcasecmp(mode, "border")) {
		return LAB_SSD_MODE_BORDER;
	} else if (!strcasecmp(mode, "full")) {
		return LAB_SSD_MODE_FULL;
	} else {
		return LAB_SSD_MODE_INVALID;
	}
}

void
ssd_set_active(struct ssd *ssd, bool active)
{
	if (!ssd) {
		return;
	}
	wlr_scene_node_set_enabled(&ssd->border.active.tree->node, active);
	wlr_scene_node_set_enabled(&ssd->titlebar.active.tree->node, active);
	if (ssd->shadow.active.tree) {
		wlr_scene_node_set_enabled(
			&ssd->shadow.active.tree->node, active);
	}
	wlr_scene_node_set_enabled(&ssd->border.inactive.tree->node, !active);
	wlr_scene_node_set_enabled(&ssd->titlebar.inactive.tree->node, !active);
	if (ssd->shadow.inactive.tree) {
		wlr_scene_node_set_enabled(
			&ssd->shadow.inactive.tree->node, !active);
	}
}

void
ssd_enable_shade(struct ssd *ssd, bool enable)
{
	if (!ssd) {
		return;
	}
	ssd_titlebar_update(ssd);
	ssd_border_update(ssd);
	wlr_scene_node_set_enabled(&ssd->extents.tree->node, !enable);
	ssd_shadow_update(ssd);
}

void
ssd_enable_keybind_inhibit_indicator(struct ssd *ssd, bool enable)
{
	if (!ssd) {
		return;
	}

	float *color = enable
		? rc.theme->window_toggled_keybinds_color
		: rc.theme->window[THEME_ACTIVE].border_color;

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
