// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include <wlr/render/pixman.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "scaled-buffer/scaled-icon-buffer.h"
#include "scaled-buffer/scaled-img-buffer.h"
#include "ssd.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

static void set_squared_corners(struct ssd *ssd, bool enable);
static void set_alt_button_icon(struct ssd *ssd, enum lab_node_type type, bool enable);
static void update_visible_buttons(struct ssd *ssd);

void
ssd_titlebar_create(struct ssd *ssd)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int width = view->current.width;
	int corner_width = ssd_get_corner_width();

	ssd->titlebar.tree = wlr_scene_tree_create(ssd->tree);
	node_descriptor_create(&ssd->titlebar.tree->node,
		LAB_NODE_TITLEBAR, view, /*data*/ NULL);

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[active];
		subtree->tree = wlr_scene_tree_create(ssd->titlebar.tree);
		struct wlr_scene_tree *parent = subtree->tree;
		wlr_scene_node_set_enabled(&parent->node, active);
		wlr_scene_node_set_position(&parent->node, 0, -theme->titlebar_height);

		struct wlr_buffer *titlebar_fill =
			&theme->window[active].titlebar_fill->base;
		struct wlr_buffer *corner_top_left =
			&theme->window[active].corner_top_left_normal->base;
		struct wlr_buffer *corner_top_right =
			&theme->window[active].corner_top_right_normal->base;

		/* Background */
		subtree->bar = wlr_scene_buffer_create(parent, titlebar_fill);
		/*
		 * Work around the wlroots/pixman bug that widened 1px buffer
		 * becomes translucent when bilinear filtering is used.
		 * TODO: remove once https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3990
		 * is solved
		 */
		if (wlr_renderer_is_pixman(view->server->renderer)) {
			wlr_scene_buffer_set_filter_mode(
				subtree->bar, WLR_SCALE_FILTER_NEAREST);
		}
		wlr_scene_node_set_position(&subtree->bar->node, corner_width, 0);

		subtree->corner_left = wlr_scene_buffer_create(parent, corner_top_left);
		wlr_scene_node_set_position(&subtree->corner_left->node,
			-rc.theme->border_width, -rc.theme->border_width);

		subtree->corner_right = wlr_scene_buffer_create(parent, corner_top_right);
		wlr_scene_node_set_position(&subtree->corner_right->node,
			width - corner_width, -rc.theme->border_width);

		/* Title */
		subtree->title = scaled_font_buffer_create_for_titlebar(
			subtree->tree, theme->titlebar_height,
			theme->window[active].titlebar_pattern);
		assert(subtree->title);
		node_descriptor_create(&subtree->title->scene_buffer->node,
			LAB_NODE_TITLE, view, /*data*/ NULL);

		/* Buttons */
		int x = theme->window_titlebar_padding_width;

		/* Center vertically within titlebar */
		int y = (theme->titlebar_height - theme->window_button_height) / 2;

		wl_list_init(&subtree->buttons_left);
		wl_list_init(&subtree->buttons_right);

		for (int b = 0; b < rc.nr_title_buttons_left; b++) {
			enum lab_node_type type = rc.title_buttons_left[b];
			struct lab_img **imgs =
				theme->window[active].button_imgs[type];
			attach_ssd_button(&subtree->buttons_left, type, parent,
				imgs, x, y, view);
			x += theme->window_button_width + theme->window_button_spacing;
		}

		x = width - theme->window_titlebar_padding_width + theme->window_button_spacing;
		for (int b = rc.nr_title_buttons_right - 1; b >= 0; b--) {
			x -= theme->window_button_width + theme->window_button_spacing;
			enum lab_node_type type = rc.title_buttons_right[b];
			struct lab_img **imgs =
				theme->window[active].button_imgs[type];
			attach_ssd_button(&subtree->buttons_right, type, parent,
				imgs, x, y, view);
		}
	}

	update_visible_buttons(ssd);

	ssd_update_title(ssd);

	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	bool squared = ssd_should_be_squared(ssd);
	if (maximized) {
		set_alt_button_icon(ssd, LAB_NODE_BUTTON_MAXIMIZE, true);
		ssd->state.was_maximized = true;
	}
	if (squared) {
		ssd->state.was_squared = true;
	}
	set_squared_corners(ssd, maximized || squared);

	if (view->shaded) {
		set_alt_button_icon(ssd, LAB_NODE_BUTTON_SHADE, true);
	}

	if (view->visible_on_all_workspaces) {
		set_alt_button_icon(ssd, LAB_NODE_BUTTON_OMNIPRESENT, true);
	}
}

static void
update_button_state(struct ssd_button *button, enum lab_button_state state,
		bool enable)
{
	if (enable) {
		button->state_set |= state;
	} else {
		button->state_set &= ~state;
	}
	/* Switch the displayed icon buffer to the new one */
	for (uint8_t state_set = LAB_BS_DEFAULT;
			state_set <= LAB_BS_ALL; state_set++) {
		struct scaled_img_buffer *buffer = button->img_buffers[state_set];
		if (!buffer) {
			continue;
		}
		wlr_scene_node_set_enabled(&buffer->scene_buffer->node,
			state_set == button->state_set);
	}
}

static void
set_squared_corners(struct ssd *ssd, bool enable)
{
	struct view *view = ssd->view;
	int width = view->current.width;
	int corner_width = ssd_get_corner_width();
	struct theme *theme = view->server->theme;

	int x = enable ? 0 : corner_width;

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[active];

		wlr_scene_node_set_position(&subtree->bar->node, x, 0);
		wlr_scene_buffer_set_dest_size(subtree->bar,
			MAX(width - 2 * x, 0), theme->titlebar_height);

		wlr_scene_node_set_enabled(&subtree->corner_left->node, !enable);

		wlr_scene_node_set_enabled(&subtree->corner_right->node, !enable);

		/* (Un)round the corner buttons */
		struct ssd_button *button;
		wl_list_for_each(button, &subtree->buttons_left, link) {
			update_button_state(button, LAB_BS_ROUNDED, !enable);
			break;
		}
		wl_list_for_each(button, &subtree->buttons_right, link) {
			update_button_state(button, LAB_BS_ROUNDED, !enable);
			break;
		}
	}
}

static void
set_alt_button_icon(struct ssd *ssd, enum lab_node_type type, bool enable)
{
	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[active];

		struct ssd_button *button;
		wl_list_for_each(button, &subtree->buttons_left, link) {
			if (button->type == type) {
				update_button_state(button,
					LAB_BS_TOGGLED, enable);
			}
		}
		wl_list_for_each(button, &subtree->buttons_right, link) {
			if (button->type == type) {
				update_button_state(button,
					LAB_BS_TOGGLED, enable);
			}
		}
	}
}

/*
 * Usually this function just enables all the nodes for buttons, but some
 * buttons can be hidden for small windows (e.g. xterm -geometry 1x1).
 */
static void
update_visible_buttons(struct ssd *ssd)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int width = MAX(view->current.width - 2 * theme->window_titlebar_padding_width, 0);
	int button_width = theme->window_button_width;
	int button_spacing = theme->window_button_spacing;
	int button_count_left = rc.nr_title_buttons_left;
	int button_count_right = rc.nr_title_buttons_right;

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

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[active];
		int button_count = 0;

		struct ssd_button *button;
		wl_list_for_each(button, &subtree->buttons_left, link) {
			wlr_scene_node_set_enabled(button->node,
				button_count < button_count_left);
			button_count++;
		}

		button_count = 0;
		wl_list_for_each(button, &subtree->buttons_right, link) {
			wlr_scene_node_set_enabled(button->node,
				button_count < button_count_right);
			button_count++;
		}
	}
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
			set_alt_button_icon(ssd, LAB_NODE_BUTTON_MAXIMIZE, maximized);
		}
		ssd->state.was_maximized = maximized;
		ssd->state.was_squared = squared;
	}

	if (ssd->state.was_shaded != view->shaded) {
		set_alt_button_icon(ssd, LAB_NODE_BUTTON_SHADE, view->shaded);
		ssd->state.was_shaded = view->shaded;
	}

	if (ssd->state.was_omnipresent != view->visible_on_all_workspaces) {
		set_alt_button_icon(ssd, LAB_NODE_BUTTON_OMNIPRESENT,
			view->visible_on_all_workspaces);
		ssd->state.was_omnipresent = view->visible_on_all_workspaces;
	}

	if (width == ssd->state.geometry.width) {
		return;
	}

	update_visible_buttons(ssd);

	/* Center buttons vertically within titlebar */
	int y = (theme->titlebar_height - theme->window_button_height) / 2;
	int x;
	int bg_offset = maximized || squared ? 0 : corner_width;

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[active];
		wlr_scene_buffer_set_dest_size(subtree->bar,
			MAX(width - bg_offset * 2, 0), theme->titlebar_height);

		x = theme->window_titlebar_padding_width;
		struct ssd_button *button;
		wl_list_for_each(button, &subtree->buttons_left, link) {
			wlr_scene_node_set_position(button->node, x, y);
			x += theme->window_button_width + theme->window_button_spacing;
		}

		x = width - corner_width;
		wlr_scene_node_set_position(&subtree->corner_right->node,
			x, -rc.theme->border_width);

		x = width - theme->window_titlebar_padding_width + theme->window_button_spacing;
		wl_list_for_each(button, &subtree->buttons_right, link) {
			x -= theme->window_button_width + theme->window_button_spacing;
			wlr_scene_node_set_position(button->node, x, y);
		}
	}

	ssd_update_title(ssd);
}

void
ssd_titlebar_destroy(struct ssd *ssd)
{
	if (!ssd->titlebar.tree) {
		return;
	}

	zfree(ssd->state.title.text);
	wlr_scene_node_destroy(&ssd->titlebar.tree->node);
	ssd->titlebar = (struct ssd_titlebar_scene){0};
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

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[active];
		struct scaled_font_buffer *title = subtree->title;
		int x, y;

		x = offset_left;
		y = (theme->titlebar_height - title->height) / 2;

		if (title_bg_width <= 0) {
			wlr_scene_node_set_enabled(&title->scene_buffer->node, false);
			continue;
		}
		wlr_scene_node_set_enabled(&title->scene_buffer->node, true);

		if (theme->window_label_text_justify == LAB_JUSTIFY_CENTER) {
			if (title->width + MAX(offset_left, offset_right) * 2 <= width) {
				/* Center based on the full width */
				x = (width - title->width) / 2;
			} else {
				/*
				 * Center based on the width between the buttons.
				 * Title jumps around once this is hit but its still
				 * better than to hide behind the buttons on the right.
				 */
				x += (title_bg_width - title->width) / 2;
			}
		} else if (theme->window_label_text_justify == LAB_JUSTIFY_RIGHT) {
			x += title_bg_width - title->width;
		} else if (theme->window_label_text_justify == LAB_JUSTIFY_LEFT) {
			/* TODO: maybe add some theme x padding here? */
		}
		wlr_scene_node_set_position(&title->scene_buffer->node, x, y);
	}
}

/*
 * Get left/right offsets of the title area based on visible/hidden states of
 * buttons set in update_visible_buttons().
 */
static void
get_title_offsets(struct ssd *ssd, int *offset_left, int *offset_right)
{
	struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[SSD_ACTIVE];
	int button_width = ssd->view->server->theme->window_button_width;
	int button_spacing = ssd->view->server->theme->window_button_spacing;
	int padding_width = ssd->view->server->theme->window_titlebar_padding_width;
	*offset_left = padding_width;
	*offset_right = padding_width;

	struct ssd_button *button;
	wl_list_for_each(button, &subtree->buttons_left, link) {
		if (button->node->enabled) {
			*offset_left += button_width + button_spacing;
		}
	}
	wl_list_for_each(button, &subtree->buttons_right, link) {
		if (button->node->enabled) {
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
	if (string_null_or_empty(view->title)) {
		return;
	}

	struct theme *theme = view->server->theme;
	struct ssd_state_title *state = &ssd->state.title;
	bool title_unchanged = state->text && !strcmp(view->title, state->text);

	int offset_left, offset_right;
	get_title_offsets(ssd, &offset_left, &offset_right);
	int title_bg_width = view->current.width - offset_left - offset_right;

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[active];
		struct ssd_state_title_width *dstate = &state->dstates[active];
		const float *text_color = theme->window[active].label_text_color;
		struct font *font = active ?
			&rc.font_activewindow : &rc.font_inactivewindow;

		if (title_bg_width <= 0) {
			dstate->truncated = true;
			continue;
		}

		if (title_unchanged
				&& !dstate->truncated && dstate->width < title_bg_width) {
			/* title the same + we don't need to resize title */
			continue;
		}

		const float bg_color[4] = {0, 0, 0, 0}; /* ignored */
		scaled_font_buffer_update(subtree->title, view->title,
			title_bg_width, font,
			text_color, bg_color);

		/* And finally update the cache */
		dstate->width = subtree->title->width;
		dstate->truncated = title_bg_width <= dstate->width;
	}

	if (!title_unchanged) {
		xstrdup_replace(state->text, view->title);
	}
	ssd_update_title_positions(ssd, offset_left, offset_right);
}

void
ssd_update_hovered_button(struct server *server, struct wlr_scene_node *node)
{
	struct ssd_button *button = NULL;

	if (node && node->data) {
		button = node_try_ssd_button_from_node(node);
		if (button == server->hovered_button) {
			/* Cursor is still on the same button */
			return;
		}
	}

	/* Disable old hover */
	if (server->hovered_button) {
		update_button_state(server->hovered_button, LAB_BS_HOVERED, false);
	}
	server->hovered_button = button;
	if (button) {
		update_button_state(button, LAB_BS_HOVERED, true);
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
