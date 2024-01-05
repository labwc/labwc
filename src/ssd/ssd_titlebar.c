// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include "buffer.h"
#include "common/mem.h"
#include "common/scaled_font_buffer.h"
#include "common/scene-helpers.h"
#include "common/string-helpers.h"
#include "labwc.h"
#include "node.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"
#include "window-rules.h"

#define FOR_EACH_STATE(ssd, tmp) FOR_EACH(tmp, \
	&(ssd)->titlebar.active, \
	&(ssd)->titlebar.inactive)

static void set_squared_corners(struct ssd *ssd, bool enable);
static void set_maximize_alt_icon(struct ssd *ssd, bool enable);

void
ssd_titlebar_create(struct ssd *ssd)
{
	struct view *view = ssd->view;
	/* Here the whole theme changing is more preferable */
	struct theme theme = get_theme_for_view(view);

	int width = view->current.width;

	float *color;
	struct wlr_scene_tree *parent;
	struct wlr_buffer *corner_top_left;
	struct wlr_buffer *corner_top_right;

	struct wlr_buffer *menu_button_unpressed;
	struct wlr_buffer *iconify_button_unpressed;
	struct wlr_buffer *maximize_button_unpressed;
	struct wlr_buffer *restore_button_unpressed;
	struct wlr_buffer *close_button_unpressed;

	struct wlr_buffer *menu_button_hover;
	struct wlr_buffer *iconify_button_hover;
	struct wlr_buffer *maximize_button_hover;
	struct wlr_buffer *restore_button_hover;
	struct wlr_buffer *close_button_hover;

	ssd->titlebar.tree = wlr_scene_tree_create(ssd->tree);

	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		subtree->tree = wlr_scene_tree_create(ssd->titlebar.tree);
		parent = subtree->tree;
		wlr_scene_node_set_position(&parent->node, 0, -theme.title_height);
		if (subtree == &ssd->titlebar.active) {
			color = theme.window_active_title_bg_color;
			corner_top_left = &theme.corner_top_left_active_normal->base;
			corner_top_right = &theme.corner_top_right_active_normal->base;
			menu_button_unpressed = &theme.button_menu_active_unpressed->base;
			iconify_button_unpressed = &theme.button_iconify_active_unpressed->base;
			close_button_unpressed = &theme.button_close_active_unpressed->base;
			maximize_button_unpressed = &theme.button_maximize_active_unpressed->base;
			restore_button_unpressed = &theme.button_restore_active_unpressed->base;

			menu_button_hover = &theme.button_menu_active_hover->base;
			iconify_button_hover = &theme.button_iconify_active_hover->base;
			close_button_hover = &theme.button_close_active_hover->base;
			maximize_button_hover = &theme.button_maximize_active_hover->base;
			restore_button_hover = &theme.button_restore_active_hover->base;
		} else {
			color = theme.window_inactive_title_bg_color;
			corner_top_left = &theme.corner_top_left_inactive_normal->base;
			corner_top_right = &theme.corner_top_right_inactive_normal->base;
			menu_button_unpressed = &theme.button_menu_inactive_unpressed->base;
			iconify_button_unpressed = &theme.button_iconify_inactive_unpressed->base;
			maximize_button_unpressed =
				&theme.button_maximize_inactive_unpressed->base;
			restore_button_unpressed = &theme.button_restore_inactive_unpressed->base;
			close_button_unpressed = &theme.button_close_inactive_unpressed->base;

			menu_button_hover = &theme.button_menu_inactive_hover->base;
			iconify_button_hover = &theme.button_iconify_inactive_hover->base;
			close_button_hover = &theme.button_close_inactive_hover->base;
			maximize_button_hover = &theme.button_maximize_inactive_hover->base;
			restore_button_hover = &theme.button_restore_inactive_hover->base;

			wlr_scene_node_set_enabled(&parent->node, false);
		}

		wl_list_init(&subtree->parts);

		/* Title */
		add_scene_rect(&subtree->parts, LAB_SSD_PART_TITLEBAR, parent,
			width - SSD_BUTTON_WIDTH * SSD_BUTTON_COUNT, theme.title_height,
			SSD_BUTTON_WIDTH, 0, color);
		/* Buttons */
		add_scene_button_corner(&subtree->parts,
			LAB_SSD_BUTTON_WINDOW_MENU, LAB_SSD_PART_CORNER_TOP_LEFT, parent,
			color, corner_top_left, menu_button_unpressed, menu_button_hover, 0, view);
		add_scene_button(&subtree->parts, LAB_SSD_BUTTON_ICONIFY, parent,
			color, iconify_button_unpressed, iconify_button_hover,
			width - SSD_BUTTON_WIDTH * 3, view);
		add_scene_button(&subtree->parts, LAB_SSD_BUTTON_MAXIMIZE, parent,
			color, maximize_button_unpressed, maximize_button_hover,
			width - SSD_BUTTON_WIDTH * 2, view);
		add_toggled_icon(&subtree->parts, LAB_SSD_BUTTON_MAXIMIZE,
			restore_button_unpressed, restore_button_hover);
		add_scene_button_corner(&subtree->parts,
			LAB_SSD_BUTTON_CLOSE, LAB_SSD_PART_CORNER_TOP_RIGHT, parent,
			color, corner_top_right, close_button_unpressed, close_button_hover,
			width - SSD_BUTTON_WIDTH * 1, view);
	} FOR_EACH_END

	ssd_update_title(ssd);

	if (view->maximized == VIEW_AXIS_BOTH) {
		set_squared_corners(ssd, true);
		set_maximize_alt_icon(ssd, true);
		ssd->state.was_maximized = true;
	}
}

static bool
is_direct_child(struct wlr_scene_node *node, struct ssd_sub_tree *subtree)
{
	return node->parent == subtree->tree;
}

static void
set_squared_corners(struct ssd *ssd, bool enable)
{
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	enum ssd_part_type ssd_type[2] = { LAB_SSD_BUTTON_WINDOW_MENU, LAB_SSD_BUTTON_CLOSE };

	FOR_EACH_STATE(ssd, subtree) {
		for (size_t i = 0; i < ARRAY_SIZE(ssd_type); i++) {
			part = ssd_get_part(&subtree->parts, ssd_type[i]);
			struct ssd_button *button = node_ssd_button_from_node(part->node);

			/* Toggle background between invisible and titlebar background color */
			struct wlr_scene_rect *rect = wlr_scene_rect_from_node(button->background);
			/*Check for custom color as well*/
			float *custom_color = window_rules_get_custom_border_color(ssd->view);
			float *rect_color;
			if (custom_color) {
				rect_color = custom_color;
			} else {
				rect_color = !enable ? (float[4]) {0, 0, 0, 0}
					: (subtree == &ssd->titlebar.active
							? rc.theme->window_active_title_bg_color
							: rc.theme->window_inactive_title_bg_color);
			}
			wlr_scene_rect_set_color(rect, rect_color);

			/* Toggle rounded corner image itself */
			struct wlr_scene_node *rounded_corner =
				wl_container_of(part->node->link.prev, rounded_corner, link);
			wlr_scene_node_set_enabled(rounded_corner, !enable);
		}
	} FOR_EACH_END
}

static void
set_maximize_alt_icon(struct ssd *ssd, bool enable)
{
	struct ssd_part *part;
	struct ssd_button *button;
	struct ssd_sub_tree *subtree;

	FOR_EACH_STATE(ssd, subtree) {
		part = ssd_get_part(&subtree->parts, LAB_SSD_BUTTON_MAXIMIZE);
		button = node_ssd_button_from_node(part->node);

		if (button->toggled) {
			wlr_scene_node_set_enabled(button->toggled, enable);
			wlr_scene_node_set_enabled(button->normal, !enable);
		}

		if (button->toggled_hover) {
			wlr_scene_node_set_enabled(button->toggled_hover, enable);
			wlr_scene_node_set_enabled(button->hover, !enable);
		}
	} FOR_EACH_END
}

void
ssd_titlebar_update(struct ssd *ssd)
{
	struct view *view = ssd->view;
	int width = view->current.width;
	struct theme *theme = view->server->theme;

	bool maximized = (view->maximized == VIEW_AXIS_BOTH);
	if (ssd->state.was_maximized != maximized) {
		set_squared_corners(ssd, maximized);
		set_maximize_alt_icon(ssd, maximized);
		ssd->state.was_maximized = maximized;
	}

	if (width == ssd->state.geometry.width) {
		return;
	}

	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		wl_list_for_each(part, &subtree->parts, link) {
			switch (part->type) {
			case LAB_SSD_PART_TITLEBAR:
				wlr_scene_rect_set_size(
					wlr_scene_rect_from_node(part->node),
					width - SSD_BUTTON_WIDTH * SSD_BUTTON_COUNT,
					theme->title_height);
				continue;
			case LAB_SSD_BUTTON_ICONIFY:
				if (is_direct_child(part->node, subtree)) {
					wlr_scene_node_set_position(part->node,
						width - SSD_BUTTON_WIDTH * 3, 0);
				}
				continue;
			case LAB_SSD_BUTTON_MAXIMIZE:
				if (is_direct_child(part->node, subtree)) {
					wlr_scene_node_set_position(part->node,
						width - SSD_BUTTON_WIDTH * 2, 0);
				}
				continue;
			case LAB_SSD_PART_CORNER_TOP_RIGHT:
				if (is_direct_child(part->node, subtree)) {
					wlr_scene_node_set_position(part->node,
						width - SSD_BUTTON_WIDTH * 1, 0);
				}
				continue;
			default:
				continue;
			}
		}
	} FOR_EACH_END
	ssd_update_title(ssd);
}

void
ssd_titlebar_destroy(struct ssd *ssd)
{
	if (!ssd->titlebar.tree) {
		return;
	}

	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		ssd_destroy_parts(&subtree->parts);
		wlr_scene_node_destroy(&subtree->tree->node);
		subtree->tree = NULL;
	} FOR_EACH_END

	if (ssd->state.title.text) {
		free(ssd->state.title.text);
		ssd->state.title.text = NULL;
	}

	wlr_scene_node_destroy(&ssd->titlebar.tree->node);
	ssd->titlebar.tree = NULL;
}

/*
 * For ssd_update_title* we do not early out because
 * .active and .inactive may result in different sizes
 * of the title (font family/size) or background of
 * the title (different button/border width).
 *
 * Both, wlr_scene_node_set_enabled() and wlr_scene_node_set_position()
 * check for actual changes and return early if there is no change in state.
 * Always using wlr_scene_node_set_enabled(node, true) will thus not cause
 * any unnecessary screen damage and makes the code easier to follow.
 */

static void
ssd_update_title_positions(struct ssd *ssd)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int width = view->current.width;
	int title_bg_width = width - SSD_BUTTON_WIDTH * SSD_BUTTON_COUNT;

	int x, y;
	int buffer_height, buffer_width;
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLE);
		if (!part || !part->node) {
			/* view->surface never been mapped */
			/* Or we somehow failed to allocate a scaled titlebar buffer */
			continue;
		}

		buffer_width = part->buffer ? part->buffer->width : 0;
		buffer_height = part->buffer ? part->buffer->height : 0;
		x = SSD_BUTTON_WIDTH;
		y = (theme->title_height - buffer_height) / 2;

		if (title_bg_width <= 0) {
			wlr_scene_node_set_enabled(part->node, false);
			continue;
		}
		wlr_scene_node_set_enabled(part->node, true);

		if (theme->window_label_text_justify == LAB_JUSTIFY_CENTER) {
			if (buffer_width + SSD_BUTTON_WIDTH * 2 <= title_bg_width) {
				/* Center based on the full width */
				x = (width - buffer_width) / 2;
			} else {
				/*
				 * Center based on the width between the buttons.
				 * Title jumps around once this is hit but its still
				 * better than to hide behind the buttons on the right.
				 */
				x += (title_bg_width - buffer_width) / 2;
			}
		} else if (theme->window_label_text_justify == LAB_JUSTIFY_RIGHT) {
			x += title_bg_width - buffer_width;
		} else if (theme->window_label_text_justify == LAB_JUSTIFY_LEFT) {
			/* TODO: maybe add some theme x padding here? */
		}
		wlr_scene_node_set_position(part->node, x, y);
	} FOR_EACH_END
}

void
ssd_update_title(struct ssd *ssd)
{
	if (!ssd) {
		return;
	}

	struct view *view = ssd->view;
	char *title = (char *)view_get_string_prop(view, "title");
	if (string_null_or_empty(title)) {
		return;
	}

	struct theme *theme = view->server->theme;
	struct ssd_state_title *state = &ssd->state.title;
	bool title_unchanged = state->text && !strcmp(title, state->text);

	float *text_color;
	struct font *font = NULL;
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	struct ssd_state_title_width *dstate;
	int title_bg_width = view->current.width
		- SSD_BUTTON_WIDTH * SSD_BUTTON_COUNT;

	FOR_EACH_STATE(ssd, subtree) {
		if (subtree == &ssd->titlebar.active) {
			dstate = &state->active;
			text_color = theme->window_active_label_text_color;
			font = &rc.font_activewindow;
		} else {
			dstate = &state->inactive;
			text_color = theme->window_inactive_label_text_color;
			font = &rc.font_inactivewindow;
		}

		if (title_bg_width <= 0) {
			dstate->truncated = true;
			continue;
		}

		if (title_unchanged
				&& !dstate->truncated && dstate->width < title_bg_width) {
			/* title the same + we don't need to resize title */
			continue;
		}

		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLE);
		if (!part) {
			/* Initialize part and wlr_scene_buffer without attaching a buffer */
			part = add_scene_part(&subtree->parts, LAB_SSD_PART_TITLE);
			part->buffer = scaled_font_buffer_create(subtree->tree);
			if (part->buffer) {
				part->node = &part->buffer->scene_buffer->node;
			} else {
				wlr_log(WLR_ERROR, "Failed to create title node");
			}
		}

		if (part->buffer) {
			scaled_font_buffer_update(part->buffer, title,
				title_bg_width, font,
				text_color, NULL);
		}

		/* And finally update the cache */
		dstate->width = part->buffer ? part->buffer->width : 0;
		dstate->truncated = title_bg_width <= dstate->width;

	} FOR_EACH_END

	if (!title_unchanged) {
		if (state->text) {
			free(state->text);
		}
		state->text = xstrdup(title);
	}
	ssd_update_title_positions(ssd);
}

static void
ssd_button_set_hover(struct ssd_button *button, bool enabled)
{
	assert(button);
	wlr_scene_node_set_enabled(&button->hover_tree->node, enabled);
	wlr_scene_node_set_enabled(&button->icon_tree->node, !enabled);
}

void
ssd_update_button_hover(struct wlr_scene_node *node,
		struct ssd_hover_state *hover_state)
{
	struct ssd_button *button = NULL;
	if (!node || !node->data) {
		goto disable_old_hover;
	}

	struct node_descriptor *desc = node->data;
	if (desc->type == LAB_NODE_DESC_SSD_BUTTON) {
		button = node_ssd_button_from_node(node);
		if (button == hover_state->button) {
			/* Cursor is still on the same button */
			return;
		}
	}

disable_old_hover:
	if (hover_state->button) {
		ssd_button_set_hover(hover_state->button, false);
		hover_state->view = NULL;
		hover_state->button = NULL;
	}
	if (button) {
		ssd_button_set_hover(button, true);
		hover_state->view = button->view;
		hover_state->button = button;
	}
}

#undef FOR_EACH_STATE
