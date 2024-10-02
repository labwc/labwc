// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include "buffer.h"
#include "config.h"
#include "common/mem.h"
#include "common/scaled-font-buffer.h"
#include "common/scene-helpers.h"
#include "common/string-helpers.h"
#if HAVE_LIBSFDO
#include "icon-loader.h"
#endif
#include "labwc.h"
#include "node.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

#define FOR_EACH_STATE(ssd, tmp) FOR_EACH(tmp, \
	&(ssd)->titlebar.active, \
	&(ssd)->titlebar.inactive)

static void set_squared_corners(struct ssd *ssd, bool enable);
static void set_alt_button_icon(struct ssd *ssd, enum ssd_part_type type, bool enable);
static void set_hover_overlays_squared(struct ssd *ssd, bool squared);
static void update_visible_buttons(struct ssd *ssd);

static void
add_button(struct ssd *ssd, struct ssd_sub_tree *subtree, enum ssd_part_type type, int x)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	struct wlr_scene_tree *parent = subtree->tree;
	bool active = subtree == &ssd->titlebar.active;

	struct ssd_part *btn_root;
	struct ssd_button *btn;

	switch (type) {
	case LAB_SSD_BUTTON_WINDOW_ICON: /* fallthrough */
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
		btn_root = add_scene_button(&subtree->parts, type, parent,
			active ? &theme->button_maximize_active_unpressed->base
				: &theme->button_maximize_inactive_unpressed->base,
			active ? &theme->button_maximize_active_hover->base
				: &theme->button_maximize_inactive_hover->base,
			x, view);
		btn = node_ssd_button_from_node(btn_root->node);
		add_toggled_icon(btn, &subtree->parts, LAB_SSD_BUTTON_MAXIMIZE,
			active ? &theme->button_restore_active_unpressed->base
				: &theme->button_restore_inactive_unpressed->base,
			active ? &theme->button_restore_active_hover->base
				: &theme->button_restore_inactive_hover->base);
		break;
	case LAB_SSD_BUTTON_SHADE:
		/* Shade button has an alternate state when shaded */
		btn_root = add_scene_button(&subtree->parts, type, parent,
			active ? &theme->button_shade_active_unpressed->base
				: &theme->button_shade_inactive_unpressed->base,
			active ? &theme->button_shade_active_hover->base
				: &theme->button_shade_inactive_hover->base,
			x, view);
		btn = node_ssd_button_from_node(btn_root->node);
		add_toggled_icon(btn, &subtree->parts, LAB_SSD_BUTTON_SHADE,
			active ? &theme->button_unshade_active_unpressed->base
				: &theme->button_unshade_inactive_unpressed->base,
			active ? &theme->button_unshade_active_hover->base
				: &theme->button_unshade_inactive_hover->base);
		break;
	case LAB_SSD_BUTTON_OMNIPRESENT:
		/* Omnipresent button has an alternate state when enabled */
		btn_root = add_scene_button(&subtree->parts, type, parent,
			active ? &theme->button_omnipresent_active_unpressed->base
				: &theme->button_omnipresent_inactive_unpressed->base,
			active ? &theme->button_omnipresent_active_hover->base
				: &theme->button_omnipresent_inactive_hover->base,
			x, view);
		btn = node_ssd_button_from_node(btn_root->node);
		add_toggled_icon(btn, &subtree->parts, LAB_SSD_BUTTON_OMNIPRESENT,
			active ? &theme->button_exclusive_active_unpressed->base
				: &theme->button_exclusive_inactive_unpressed->base,
			active ? &theme->button_exclusive_active_hover->base
				: &theme->button_exclusive_inactive_hover->base);
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
	int corner_width = ssd_get_corner_width();

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
			width - corner_width * 2, theme->title_height,
			corner_width, 0, color);
		add_scene_buffer(&subtree->parts, LAB_SSD_PART_TITLEBAR_CORNER_LEFT, parent,
			corner_top_left, -rc.theme->border_width, -rc.theme->border_width);
		add_scene_buffer(&subtree->parts, LAB_SSD_PART_TITLEBAR_CORNER_RIGHT, parent,
			corner_top_right, width - corner_width,
			-rc.theme->border_width);

		/* Buttons */
		struct title_button *b;
		int x = theme->padding_width;
		wl_list_for_each(b, &rc.title_buttons_left, link) {
			add_button(ssd, subtree, b->type, x);
			x += theme->window_button_width + theme->window_button_spacing;
		}

		x = width - theme->padding_width + theme->window_button_spacing;
		wl_list_for_each_reverse(b, &rc.title_buttons_right, link) {
			x -= theme->window_button_width + theme->window_button_spacing;
			add_button(ssd, subtree, b->type, x);
		}
	} FOR_EACH_END

	update_visible_buttons(ssd);
	set_hover_overlays_squared(ssd, false);

	ssd_update_title(ssd);
	ssd_update_window_icon(ssd);

	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	if (maximized) {
		set_squared_corners(ssd, true);
		set_alt_button_icon(ssd, LAB_SSD_BUTTON_MAXIMIZE, true);
		ssd->state.was_maximized = true;
	}

	if (view->shaded) {
		set_alt_button_icon(ssd, LAB_SSD_BUTTON_SHADE, true);
	}

	if (view->visible_on_all_workspaces) {
		set_alt_button_icon(ssd, LAB_SSD_BUTTON_OMNIPRESENT, true);
	}

	if (ssd_should_be_squared(ssd)) {
		set_squared_corners(ssd, true);
		ssd->state.was_squared = true;
	}
}

static void
set_squared_corners(struct ssd *ssd, bool enable)
{
	struct view *view = ssd->view;
	int width = view->current.width;
	int corner_width = ssd_get_corner_width();
	struct theme *theme = view->server->theme;

	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	int x = enable ? 0 : corner_width;

	FOR_EACH_STATE(ssd, subtree) {
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
		wlr_scene_node_set_position(part->node, x, 0);
		wlr_scene_rect_set_size(
			wlr_scene_rect_from_node(part->node), width - 2 * x, theme->title_height);

		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR_CORNER_LEFT);
		wlr_scene_node_set_enabled(part->node, !enable);

		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR_CORNER_RIGHT);
		wlr_scene_node_set_enabled(part->node, !enable);
	} FOR_EACH_END

	set_hover_overlays_squared(ssd, enable);
}

static void
set_alt_button_icon(struct ssd *ssd, enum ssd_part_type type, bool enable)
{
	struct ssd_part *part;
	struct ssd_button *button;
	struct ssd_sub_tree *subtree;

	FOR_EACH_STATE(ssd, subtree) {
		part = ssd_get_part(&subtree->parts, type);
		if (!part) {
			return;
		}

		button = node_ssd_button_from_node(part->node);

		if (button->toggled) {
			wlr_scene_node_set_enabled(&button->toggled_tree->node, enable);
			wlr_scene_node_set_enabled(&button->untoggled_tree->node, !enable);
		}
	} FOR_EACH_END
}

/*
 * Usually this function just enables all the nodes for buttons, but some
 * buttons can be hidden for small windows (e.g. xterm -geometry 1x1).
 */
static void
update_visible_buttons(struct ssd *ssd)
{
	struct view *view = ssd->view;
	int width = view->current.width - (2 * view->server->theme->padding_width);
	int button_width = view->server->theme->window_button_width;
	int button_spacing = view->server->theme->window_button_spacing;
	int button_count_left = wl_list_length(&rc.title_buttons_left);
	int button_count_right = wl_list_length(&rc.title_buttons_right);

	/* Make sure infinite loop never occurs */
	assert(button_width > 0);

	/*
	 * The corner-left button is lastly removed as it's usually a window
	 * menu button (or an app icon button in the future).
	 *
	 * There is spacing to the inside of each button, including between the
	 * innermost buttons and the window title. See also get_title_offsets().
	 */
	while (width < ((button_width + button_spacing)
			* (button_count_left + button_count_right))) {
		if (button_count_left > button_count_right) {
			button_count_left--;
		} else {
			button_count_right--;
		}
	}

	int button_count;
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	struct title_button *b;
	FOR_EACH_STATE(ssd, subtree) {
		button_count = 0;
		wl_list_for_each(b, &rc.title_buttons_left, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			wlr_scene_node_set_enabled(part->node,
				button_count < button_count_left);
			button_count++;
		}

		button_count = 0;
		wl_list_for_each_reverse(b, &rc.title_buttons_right, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			wlr_scene_node_set_enabled(part->node,
				button_count < button_count_right);
			button_count++;
		}
	} FOR_EACH_END
}

void
ssd_titlebar_update(struct ssd *ssd)
{
	struct view *view = ssd->view;
	int width = view->current.width;
	int corner_width = ssd_get_corner_width();
	struct theme *theme = view->server->theme;

	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	bool squared = ssd_should_be_squared(ssd);

	if (ssd->state.was_maximized != maximized
			|| ssd->state.was_squared != squared) {
		set_squared_corners(ssd, maximized || squared);
		if (ssd->state.was_maximized != maximized) {
			set_alt_button_icon(ssd, LAB_SSD_BUTTON_MAXIMIZE, maximized);
		}
		ssd->state.was_maximized = maximized;
		ssd->state.was_squared = squared;
	}

	if (ssd->state.was_shaded != view->shaded) {
		set_alt_button_icon(ssd, LAB_SSD_BUTTON_SHADE, view->shaded);
		ssd->state.was_shaded = view->shaded;
	}

	if (ssd->state.was_omnipresent != view->visible_on_all_workspaces) {
		set_alt_button_icon(ssd, LAB_SSD_BUTTON_OMNIPRESENT,
			view->visible_on_all_workspaces);
		ssd->state.was_omnipresent = view->visible_on_all_workspaces;
	}

	if (width == ssd->state.geometry.width) {
		return;
	}

	update_visible_buttons(ssd);

	int x;
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;
	struct title_button *b;
	int bg_offset = maximized || squared ? 0 : corner_width;
	FOR_EACH_STATE(ssd, subtree) {
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
		wlr_scene_rect_set_size(
			wlr_scene_rect_from_node(part->node),
			width - bg_offset * 2, theme->title_height);

		x = theme->padding_width;
		wl_list_for_each(b, &rc.title_buttons_left, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			wlr_scene_node_set_position(part->node, x, 0);
			x += theme->window_button_width + theme->window_button_spacing;
		}

		x = width - corner_width;
		part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR_CORNER_RIGHT);
		wlr_scene_node_set_position(part->node, x, -rc.theme->border_width);

		x = width - theme->padding_width + theme->window_button_spacing;
		wl_list_for_each_reverse(b, &rc.title_buttons_right, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			x -= theme->window_button_width + theme->window_button_spacing;
			wlr_scene_node_set_position(part->node, x, 0);
		}
	} FOR_EACH_END

	ssd_update_title(ssd);
	ssd_update_window_icon(ssd);
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
		zfree(ssd->state.title.text);
	}
	if (ssd->state.app_id) {
		zfree(ssd->state.app_id);
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
ssd_update_title_positions(struct ssd *ssd, int offset_left, int offset_right)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int width = view->current.width;
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

/*
 * Get left/right offsets of the title area based on visible/hidden states of
 * buttons set in update_visible_buttons().
 */
static void
get_title_offsets(struct ssd *ssd, int *offset_left, int *offset_right)
{
	struct ssd_sub_tree *subtree = &ssd->titlebar.active;
	int button_width = ssd->view->server->theme->window_button_width;
	int button_spacing = ssd->view->server->theme->window_button_spacing;
	int padding_width = ssd->view->server->theme->padding_width;
	*offset_left = padding_width;
	*offset_right = padding_width;

	struct title_button *b;
	wl_list_for_each(b, &rc.title_buttons_left, link) {
		struct ssd_part *part = ssd_get_part(&subtree->parts, b->type);
		if (part->node->enabled) {
			*offset_left += button_width + button_spacing;
		}
	}
	wl_list_for_each_reverse(b, &rc.title_buttons_right, link) {
		struct ssd_part *part = ssd_get_part(&subtree->parts, b->type);
		if (part->node->enabled) {
			*offset_right += button_width + button_spacing;
		}
	}
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

	int offset_left, offset_right;
	get_title_offsets(ssd, &offset_left, &offset_right);
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
	ssd_update_title_positions(ssd, offset_left, offset_right);
}

static bool
is_hover_overlay_buffer(struct wlr_scene_node *node)
{
	struct wlr_scene_buffer *scene_buffer =
		wlr_scene_buffer_from_node(node);
	return scene_buffer->buffer == &rc.theme->button_hover_overlay_left->base
		|| scene_buffer->buffer == &rc.theme->button_hover_overlay_right->base
		|| scene_buffer->buffer == &rc.theme->button_hover_overlay_middle->base;
}

static struct wlr_buffer *
hover_overlay_for_corner(enum ssd_corner corner)
{
	switch (corner) {
	case LAB_CORNER_TOP_LEFT:
		return &rc.theme->button_hover_overlay_left->base;
	case LAB_CORNER_TOP_RIGHT:
		return &rc.theme->button_hover_overlay_right->base;
	case LAB_CORNER_UNKNOWN:
		return &rc.theme->button_hover_overlay_middle->base;
	default:
		assert(false);
		abort();
	}
}

static void
set_hover_overlay(struct ssd_button *button, enum ssd_corner corner)
{
	assert(button);

	/*
	 * When button->(toggled_)hover is the builtin hover overlay (not an
	 * icon provided by user), update its shape.
	 */
	if (is_hover_overlay_buffer(button->hover)) {
		wlr_scene_buffer_set_buffer(
			wlr_scene_buffer_from_node(button->hover),
			hover_overlay_for_corner(corner));
	}
	if (button->toggled_hover
			&& is_hover_overlay_buffer(button->toggled_hover)) {
		wlr_scene_buffer_set_buffer(
			wlr_scene_buffer_from_node(button->toggled_hover),
			hover_overlay_for_corner(corner));
	}
}

/* Update the shape of hover overlay on corner buttons */
static void
set_hover_overlays_squared(struct ssd *ssd, bool squared)
{
	struct ssd_part *part;
	struct ssd_sub_tree *subtree;

	FOR_EACH_STATE(ssd, subtree) {
		struct title_button *b;
		wl_list_for_each(b, &rc.title_buttons_left, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			struct ssd_button *button = node_ssd_button_from_node(part->node);
			set_hover_overlay(button,
				squared ? LAB_CORNER_UNKNOWN : LAB_CORNER_TOP_LEFT);
			break;
		}
		wl_list_for_each_reverse(b, &rc.title_buttons_right, link) {
			part = ssd_get_part(&subtree->parts, b->type);
			struct ssd_button *button = node_ssd_button_from_node(part->node);
			set_hover_overlay(button,
				squared ? LAB_CORNER_UNKNOWN : LAB_CORNER_TOP_RIGHT);
			break;
		}
	} FOR_EACH_END
}

static void
ssd_button_set_hover(struct ssd_button *button, bool enabled)
{
	assert(button);

	/*
	 * Keep showing non-hover icon when the hover icon is not provided and
	 * hover overlay is shown on top of it.
	 */
	wlr_scene_node_set_enabled(button->normal,
		is_hover_overlay_buffer(button->hover) || !enabled);
	wlr_scene_node_set_enabled(button->hover, enabled);

	if (button->toggled) {
		wlr_scene_node_set_enabled(button->toggled,
			is_hover_overlay_buffer(button->toggled_hover) || !enabled);
		wlr_scene_node_set_enabled(button->toggled_hover, enabled);
	}
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

bool
ssd_should_be_squared(struct ssd *ssd)
{
	struct view *view = ssd->view;
	int corner_width = ssd_get_corner_width();

	return (view_is_tiled_and_notify_tiled(view)
			|| view->current.width < corner_width * 2)
		&& view->maximized != VIEW_AXIS_BOTH;
}

void
ssd_update_window_icon(struct ssd *ssd)
{
#if HAVE_LIBSFDO
	if (!ssd) {
		return;
	}

	const char *app_id = view_get_string_prop(ssd->view, "app_id");
	if (string_null_or_empty(app_id)) {
		return;
	}
	if (ssd->state.app_id && !strcmp(ssd->state.app_id, app_id)) {
		return;
	}

	free(ssd->state.app_id);
	ssd->state.app_id = xstrdup(app_id);

	struct theme *theme = ssd->view->server->theme;

	int icon_size = MIN(theme->window_button_width,
		theme->title_height - 2 * theme->padding_height);
	/* TODO: take into account output scales */
	int icon_scale = 1;

	struct lab_data_buffer *icon_buffer = icon_loader_lookup(
		ssd->view->server, app_id, icon_size, icon_scale);
	if (!icon_buffer) {
		wlr_log(WLR_DEBUG, "icon could not be loaded for %s", app_id);
		return;
	}

	struct ssd_sub_tree *subtree;
	FOR_EACH_STATE(ssd, subtree) {
		struct ssd_part *part =
			ssd_get_part(&subtree->parts, LAB_SSD_BUTTON_WINDOW_ICON);
		if (!part) {
			break;
		}

		struct ssd_button *button = node_ssd_button_from_node(part->node);
		update_window_icon_buffer(button->normal, &icon_buffer->base);
		update_window_icon_buffer(button->hover, &icon_buffer->base);
	} FOR_EACH_END

	wlr_buffer_drop(&icon_buffer->base);
#endif
}

#undef FOR_EACH_STATE
