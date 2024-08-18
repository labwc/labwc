// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include "buffer.h"
#include "common/mem.h"
#include "common/scaled-font-buffer.h"
#include "common/scene-helpers.h"
#include "common/string-helpers.h"
#include "labwc.h"
#include "node.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

#define FOR_EACH_STATE(ssd, tmp) FOR_EACH(tmp, \
	&(ssd)->titlebar.active, \
	&(ssd)->titlebar.inactive)

static void set_squared_corners(struct ssd *ssd, bool enable);
static void set_maximize_alt_icon(struct ssd *ssd, bool enable);

static void
add_button(struct ssd *ssd, struct ssd_sub_tree *subtree, enum ssd_part_type type, int x)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	struct wlr_scene_tree *parent = subtree->tree;
	bool active = subtree == &ssd->titlebar.active;

	struct ssd_part *btn_max_root;
	struct ssd_button *btn_max;

	switch (type) {
	case LAB_SSD_BUTTON_WINDOW_MENU:
		add_scene_button(&subtree->parts, type, parent,
			active ? &theme->button_menu_active_unpressed->base
				: &theme->button_menu_inactive_unpressed->base,
			active ? &theme->button_menu_active_hover->base
				: &theme->button_menu_inactive_hover->base,
			x, view);
		break;
	case LAB_SSD_BUTTON_ICONIFY:
		add_scene_button(&subtree->parts, type, parent,
			active ? &theme->button_iconify_active_unpressed->base
				: &theme->button_iconify_inactive_unpressed->base,
			active ? &theme->button_iconify_active_hover->base
				: &theme->button_iconify_inactive_hover->base,
			x, view);
		break;
	case LAB_SSD_BUTTON_MAXIMIZE:
		/* Maximize button has an alternate state when maximized */
		btn_max_root = add_scene_button(&subtree->parts, type, parent,
			active ? &theme->button_maximize_active_unpressed->base
				: &theme->button_maximize_inactive_unpressed->base,
			active ? &theme->button_maximize_active_hover->base
				: &theme->button_maximize_inactive_hover->base,
			x, view);
		btn_max = node_ssd_button_from_node(btn_max_root->node);
		add_toggled_icon(btn_max, &subtree->parts, LAB_SSD_BUTTON_MAXIMIZE,
			active ? &theme->button_restore_active_unpressed->base
				: &theme->button_restore_inactive_unpressed->base,
			active ? &theme->button_restore_active_hover->base
				: &theme->button_restore_inactive_hover->base);
		break;
	case LAB_SSD_BUTTON_CLOSE:
		add_scene_button(&subtree->parts, type, parent,
			active ? &theme->button_close_active_unpressed->base
				: &theme->button_close_inactive_unpressed->base,
			active ? &theme->button_close_active_hover->base
				: &theme->button_close_inactive_hover->base,
			x, view);
		break;
	default:
		assert(false && "invalid titlebar part");
		wlr_log(WLR_ERROR, "invalid titlebar type");
		abort();
	}
}

void
ssd_titlebar_create(struct ssd *ssd)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int width = view->current.width;

	float *color;
	struct wlr_scene_tree *parent;
	struct wlr_buffer *corner_top_left;
	struct wlr_buffer *corner_top_right;

	ssd->titlebar.tree = wlr_scene_tree_create(ssd->tree);

	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		subtree->tree = wlr_scene_tree_create(ssd->titlebar.tree);
		parent = subtree->tree;
		wlr_scene_node_set_position(&parent->node, 0, -theme->title_height);
		if (subtree == &ssd->titlebar.active) {
			color = theme->window_active_title_bg_color;
			corner_top_left = &theme->corner_top_left_active_normal->base;
			corner_top_right = &theme->corner_top_right_active_normal->base;
		} else {
			color = theme->window_inactive_title_bg_color;
			corner_top_left = &theme->corner_top_left_inactive_normal->base;
			corner_top_right = &theme->corner_top_right_inactive_normal->base;

			wlr_scene_node_set_enabled(&parent->node, false);
		}
		wl_list_init(&subtree->parts);

		/* Background */
		add_scene_rect(&subtree->parts, LAB_SSD_PART_TITLEBAR, parent,
			width - theme->window_button_width * 2, theme->title_height,
			theme->window_button_width, 0, color);
		add_scene_buffer(&subtree->parts, LAB_SSD_PART_CORNER_TOP_LEFT, parent,
			corner_top_left, -rc.theme->border_width, -rc.theme->border_width);
		add_scene_buffer(&subtree->parts, LAB_SSD_PART_CORNER_TOP_RIGHT, parent,
			corner_top_right, width - theme->window_button_width,
			-rc.theme->border_width);

		/* Buttons */
		struct title_button *b;
		int x = 0;
		wl_list_for_each(b, &rc.title_buttons_left, link) {
			add_button(ssd, subtree, b->type, x);
			x += theme->window_button_width;
		}

		x = width;
		wl_list_for_each_reverse(b, &rc.title_buttons_right, link) {
			x -= theme->window_button_width;
			add_button(ssd, subtree, b->type, x);
		}
	} FOR_EACH_END

	ssd_update_title(ssd);

	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	if (maximized) {
		set_squared_corners(ssd, true);
		set_maximize_alt_icon(ssd, true);
		ssd->state.was_maximized = true;
	}
	if (view_is_tiled_and_notify_tiled(view) && !maximized) {
		set_squared_corners(ssd, true);
		ssd->state.was_tiled_not_maximized = true;
	}
}

static void
set_squared_corners(struct ssd *ssd, bool enable)
{
	struct view *view = ssd->view;
	int width = view->current.width;
	struct theme *theme = view->server->theme;

	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	int x = enable ? 0 : theme->window_button_width;

	FOR_EACH_STATE(ssd, subtree) {
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
		wlr_scene_node_set_position(part->node, x, 0);
		wlr_scene_rect_set_size(
			wlr_scene_rect_from_node(part->node), width - 2 * x, theme->title_height);

		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_CORNER_TOP_LEFT);
		wlr_scene_node_set_enabled(part->node, !enable);

		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_CORNER_TOP_RIGHT);
		wlr_scene_node_set_enabled(part->node, !enable);
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
		if (!part) {
			return;
		}

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

	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	bool tiled_not_maximized = view_is_tiled_and_notify_tiled(ssd->view)
		&& !maximized;

	if (ssd->state.was_maximized != maximized
			|| ssd->state.was_tiled_not_maximized != tiled_not_maximized) {
		set_squared_corners(ssd, maximized || tiled_not_maximized);
		if (ssd->state.was_maximized != maximized) {
			set_maximize_alt_icon(ssd, maximized);
		}
		ssd->state.was_maximized = maximized;
		ssd->state.was_tiled_not_maximized = tiled_not_maximized;
	}

	if (width == ssd->state.geometry.width) {
		return;
	}

	int x;
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	struct title_button *b;
	int bg_offset = maximized || tiled_not_maximized ? 0 : theme->window_button_width;
	FOR_EACH_STATE(ssd, subtree) {
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
		wlr_scene_rect_set_size(
			wlr_scene_rect_from_node(part->node),
			width - bg_offset * 2, theme->title_height);

		x = 0;
		wl_list_for_each(b, &rc.title_buttons_left, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			wlr_scene_node_set_position(part->node, x, 0);
			x += theme->window_button_width;
		}

		x = width - theme->window_button_width;
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_CORNER_TOP_RIGHT);
		wlr_scene_node_set_position(part->node, x, -rc.theme->border_width);
		wl_list_for_each_reverse(b, &rc.title_buttons_right, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			wlr_scene_node_set_position(part->node, x, 0);
			x -= theme->window_button_width;
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
	int offset_left = theme->window_button_width * wl_list_length(&rc.title_buttons_left);
	int offset_right = theme->window_button_width * wl_list_length(&rc.title_buttons_right);
	int title_bg_width = width - offset_left - offset_right;

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
		x = offset_left;
		y = (theme->title_height - buffer_height) / 2;

		if (title_bg_width <= 0) {
			wlr_scene_node_set_enabled(part->node, false);
			continue;
		}
		wlr_scene_node_set_enabled(part->node, true);

		if (theme->window_label_text_justify == LAB_JUSTIFY_CENTER) {
			if (buffer_width + MAX(offset_left, offset_right) * 2 <= width) {
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
	if (!ssd || !rc.show_title) {
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

	const float *text_color;
	const float *bg_color;
	struct font *font = NULL;
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	struct ssd_state_title_width *dstate;
	int offset_left = theme->window_button_width * wl_list_length(&rc.title_buttons_left);
	int offset_right = theme->window_button_width * wl_list_length(&rc.title_buttons_right);
	int title_bg_width = view->current.width - offset_left - offset_right;

	FOR_EACH_STATE(ssd, subtree) {
		if (subtree == &ssd->titlebar.active) {
			dstate = &state->active;
			text_color = theme->window_active_label_text_color;
			bg_color = theme->window_active_title_bg_color;
			font = &rc.font_activewindow;
		} else {
			dstate = &state->inactive;
			text_color = theme->window_inactive_label_text_color;
			bg_color = theme->window_inactive_title_bg_color;
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
				text_color, bg_color, NULL);
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
