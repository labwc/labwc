// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include "labwc.h"
#include "ssd.h"
#include "theme.h"
#include "common/font.h"
#include "common/scene-helpers.h"

#define FOR_EACH_STATE(view, tmp) FOR_EACH(tmp, \
	&(view)->ssd.titlebar.active, \
	&(view)->ssd.titlebar.inactive)

void
ssd_titlebar_create(struct view *view)
{
	struct theme *theme = view->server->theme;
	int width = view->w;
	int full_width = width + 2 * theme->border_width;

	float *color;
	struct wlr_scene_node *parent;
	struct wlr_buffer *corner_top_left;
	struct wlr_buffer *corner_top_right;

	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(view, subtree) {
		subtree->tree = wlr_scene_tree_create(&view->ssd.tree->node);
		parent = &subtree->tree->node;
		wlr_scene_node_set_position(parent, -theme->border_width, -SSD_HEIGHT);
		if (subtree == &view->ssd.titlebar.active) {
			color = theme->window_active_title_bg_color;
			corner_top_left = &theme->corner_top_left_active_normal->base;
			corner_top_right = &theme->corner_top_right_active_normal->base;
		} else {
			color = theme->window_inactive_title_bg_color;
			corner_top_left = &theme->corner_top_left_inactive_normal->base;
			corner_top_right = &theme->corner_top_right_inactive_normal->base;
			wlr_scene_node_set_enabled(parent, false);
		}
		wl_list_init(&subtree->parts);

		/* Title */
		add_scene_rect(&subtree->parts, LAB_SSD_PART_TITLEBAR, parent,
			full_width - BUTTON_WIDTH * BUTTON_COUNT, SSD_HEIGHT,
			BUTTON_WIDTH, 0, color);
		/* Buttons */
		add_scene_button_corner(&subtree->parts, LAB_SSD_BUTTON_WINDOW_MENU,
			parent,	corner_top_left,
			&theme->xbm_menu_active_unpressed->base, 0);
		add_scene_button(&subtree->parts, LAB_SSD_BUTTON_ICONIFY, parent,
			color, &theme->xbm_iconify_active_unpressed->base,
			full_width - BUTTON_WIDTH * 3);
		add_scene_button(&subtree->parts, LAB_SSD_BUTTON_MAXIMIZE, parent,
			color, &theme->xbm_maximize_active_unpressed->base,
			full_width - BUTTON_WIDTH * 2);
		add_scene_button_corner(&subtree->parts, LAB_SSD_BUTTON_CLOSE, parent,
			corner_top_right, &theme->xbm_close_active_unpressed->base,
			full_width - BUTTON_WIDTH * 1);
	} FOR_EACH_END
	ssd_update_title(view);
}

static bool
is_direct_child(struct wlr_scene_node *node, struct ssd_sub_tree *subtree)
{
	return node->parent == &subtree->tree->node;
}

void
ssd_titlebar_update(struct view *view)
{
	int width = view->w;
	if (width == view->ssd.state.width) {
		return;
	}
	int full_width = width + 2 * view->server->theme->border_width;

	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(view, subtree) {
		wl_list_for_each(part, &subtree->parts, link) {
			switch (part->type) {
			case LAB_SSD_PART_TITLEBAR:
				wlr_scene_rect_set_size(lab_wlr_scene_get_rect(part->node),
					full_width - BUTTON_WIDTH * BUTTON_COUNT, SSD_HEIGHT);
				continue;
			case LAB_SSD_BUTTON_ICONIFY:
				if (is_direct_child(part->node, subtree)) {
					wlr_scene_node_set_position(part->node,
						full_width - BUTTON_WIDTH * 3, 0);
				}
				continue;
			case  LAB_SSD_BUTTON_MAXIMIZE:
				if (is_direct_child(part->node, subtree)) {
					wlr_scene_node_set_position(part->node,
						full_width - BUTTON_WIDTH * 2, 0);
				}
				continue;
			case LAB_SSD_BUTTON_CLOSE:
				if (is_direct_child(part->node, subtree)) {
					wlr_scene_node_set_position(part->node,
						full_width - BUTTON_WIDTH * 1, 0);
				}
				continue;
			default:
				continue;
			}
		}
	} FOR_EACH_END
	ssd_update_title(view);
}

void
ssd_titlebar_destroy(struct view *view)
{
	if (!view->ssd.titlebar.active.tree) {
		return;
	}

	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(view, subtree) {
		ssd_destroy_parts(&subtree->parts);
		wlr_scene_node_destroy(&subtree->tree->node);
		subtree->tree = NULL;
	} FOR_EACH_END

	if (view->ssd.state.title.text) {
		free(view->ssd.state.title.text);
		view->ssd.state.title.text = NULL;
	}
}

/*
 * For ssd_update_title* we do not early out because
 * .active and .inactive may result in different sizes
 * of the title (font family/size) or background of
 * the title (different button/border width).
 */

static void
ssd_update_title_positions(struct view *view)
{
	struct theme *theme = view->server->theme;
	int width = view->w;
	int full_width = width + 2 * view->server->theme->border_width;

	int x, y;
	struct wlr_scene_rect *rect;
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(view, subtree) {
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLE);
		if (!part) {
			/* view->surface never been mapped */
			continue;
		}

		x = 0;
		y = (SSD_HEIGHT - part->buffer->base.height) / 2;
		rect = lab_wlr_scene_get_rect(part->node->parent);
		if (rect->width <= 0) {
			wlr_scene_node_set_position(part->node, x, y);
			continue;
		}

		if (theme->window_label_text_justify == LAB_JUSTIFY_CENTER) {
			if (part->buffer->base.width + BUTTON_WIDTH * 2 <= rect->width) {
				/* Center based on the full width */
				x = (full_width - part->buffer->base.width) / 2;
				x -= BUTTON_WIDTH;
			} else {
				/*
				 * Center based on the width between the buttons.
				 * Title jumps around once this is hit but its still
				 * better than to hide behind the buttons on the right.
				 */
				x = (rect->width - part->buffer->base.width) / 2;
			}
		} else if (theme->window_label_text_justify == LAB_JUSTIFY_RIGHT) {
			x = rect->width - part->buffer->base.width;
		} else if (theme->window_label_text_justify == LAB_JUSTIFY_LEFT) {
			/* TODO: maybe add some theme x padding here? */
		}
		wlr_scene_node_set_position(part->node, x, y);
	} FOR_EACH_END
}

void
ssd_update_title(struct view *view)
{
	if (!view->ssd.tree) {
		return;
	}

	char *title = (char *)view_get_string_prop(view, "title");
	if (!title || !*title) {
		return;
	}

	struct theme *theme = view->server->theme;
	struct ssd_state_title *state = &view->ssd.state.title;
	bool title_unchanged = state->text && !strcmp(title, state->text);

	/* TODO: Do we only have active window fonts? */
	struct font font = {
		.name = rc.font_name_activewindow,
		.size = rc.font_size_activewindow,
	};

	float *text_color;
	struct wlr_scene_rect *rect;
	struct ssd_part *part;
	struct ssd_part *parent_part;
	struct ssd_sub_tree *subtree;
	struct ssd_state_title_width *dstate;
	FOR_EACH_STATE(view, subtree) {
		parent_part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
		assert(parent_part);

		if (subtree == &view->ssd.titlebar.active) {
			dstate = &state->active;
			text_color = theme->window_active_label_text_color;
		} else {
			dstate = &state->inactive;
			text_color = theme->window_inactive_label_text_color;
		}

		rect = lab_wlr_scene_get_rect(parent_part->node);
		if (rect->width <= 0) {
			dstate->truncated = true;
			continue;
		}

		if (title_unchanged
				&& !dstate->truncated && dstate->width < rect->width) {
			/* title the same + we don't need to resize title */
			continue;
		}

		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLE);
		if (!part) {
			part = add_scene_part(&subtree->parts, LAB_SSD_PART_TITLE);
		}
		if (part->node) {
			wlr_scene_node_destroy(part->node);
		}
		font_buffer_update(
			&part->buffer, rect->width, title, &font, text_color);
		part->node = &wlr_scene_buffer_create(
			parent_part->node, &part->buffer->base)->node;
		dstate->width = part->buffer->base.width;
		dstate->truncated = rect->width <= dstate->width;
	} FOR_EACH_END

	if (state->text) {
		free(state->text);
	}
	state->text = strdup(title);
	ssd_update_title_positions(view);
}

/*
 * Returns the wlr_scene_node for hover effect.
 * To disable the hover effect later on just call
 * wlr_scene_node_set_enabled(node, false).
 */
struct wlr_scene_node *
ssd_button_hover_enable(struct view *view, enum ssd_part_type type)
{
	if (!view->ssd.tree) {
		wlr_log(WLR_ERROR, "%s() for destroyed view", __func__);
		return NULL;
	}

	assert(ssd_is_button(type));
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(view, subtree) {
		if (subtree->tree->node.state.enabled) {
			part = ssd_get_part(&subtree->parts, type);
			if (!part) {
				wlr_log(WLR_ERROR, "hover enable failed to find button");
				return NULL;
			}
			struct wlr_scene_node *child;
			wl_list_for_each(child, &part->node->state.children, state.link) {
				if (child->type == WLR_SCENE_NODE_RECT) {
					wlr_scene_node_set_enabled(child, true);
					return child;
				}
			}
		}
	} FOR_EACH_END

	wlr_log(WLR_ERROR, "hover enable failed to find button");
	return NULL;
}

#undef FOR_EACH_STATE
