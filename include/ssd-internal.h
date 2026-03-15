/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SSD_INTERNAL_H
#define LABWC_SSD_INTERNAL_H

#include <wlr/util/box.h>
#include "common/border.h"
#include "theme.h"
#include "view.h"

struct ssd_state_title_width {
	int width;
	bool truncated;
};

/*
 * The scene-graph of SSD looks like below. The parentheses indicate the
 * type of each node (enum lab_node_type, stored in the node_descriptor
 * attached to the wlr_scene_node).
 *
 * ssd->tree (LAB_NODE_SSD_ROOT)
 * +--titlebar (LAB_NODE_TITLEBAR)
 * |  +--inactive
 * |  |  +--background bar
 * |  |  +--left corner
 * |  |  +--right corner
 * |  |  +--title (LAB_NODE_TITLE)
 * |  |  +--iconify button (LAB_NODE_BUTTON_ICONIFY)
 * |  |  |  +--normal close icon image
 * |  |  |  +--hovered close icon image
 * |  |  |  +--...
 * |  |  +--window icon (LAB_NODE_BUTTON_WINDOW_ICON)
 * |  |  |  +--window icon image
 * |  |  +--...
 * |  +--active
 * |     +--...
 * +--border
 * |  +--inactive
 * |  |  +--top
 * |  |  +--...
 * |  +--active
 * |     +--top
 * |     +--...
 * +--shadow
 * |  +--inactive
 * |  |  +--top
 * |  |  +--...
 * |  +--active
 * |     +--top
 * |     +--...
 * +--extents
 *    +--top
 *    +--...
 */
struct ssd {
	struct view *view;
	struct wlr_scene_tree *tree;

	/*
	 * Cache for current values.
	 * Used to detect actual changes so we
	 * don't update things we don't have to.
	 */
	struct {
		/* Button icons need to be swapped on shade or omnipresent toggles */
		bool was_shaded;
		bool was_omnipresent;

		/*
		 * Corners need to be (un)rounded and borders need be shown/hidden
		 * when toggling maximization, and the button needs to be swapped on
		 * maximization toggles.
		 */
		bool was_maximized;

		/*
		 * Corners need to be (un)rounded but borders should be kept shown when
		 * the window is (un)tiled and notified about it or when the window may
		 * become so small that only a squared scene-rect can be used to render
		 * such a small titlebar.
		 */
		bool was_squared;

		struct wlr_box geometry;
		struct ssd_state_title {
			char *text;
			/* indexed by enum ssd_active_state */
			struct ssd_state_title_width dstates[2];
		} title;
	} state;

	/* An invisible area around the view which allows resizing */
	struct ssd_extents_scene {
		struct wlr_scene_tree *tree;
		struct wlr_scene_rect *top, *bottom, *left, *right;
	} extents;

	/* The top of the view, containing buttons, title, .. */
	struct ssd_titlebar_scene {
		int height;
		struct wlr_scene_tree *tree;
		struct ssd_titlebar_subtree {
			struct wlr_scene_tree *tree;
			struct wlr_scene_buffer *corner_left;
			struct wlr_scene_buffer *corner_right;
			struct wlr_scene_buffer *bar;
			struct scaled_font_buffer *title;
			struct wl_list buttons_left; /* ssd_button.link */
			struct wl_list buttons_right; /* ssd_button.link */
		} subtrees[2]; /* indexed by enum ssd_active_state */
	} titlebar;

	/* Borders allow resizing as well */
	struct ssd_border_scene {
		struct wlr_scene_tree *tree;
		struct ssd_border_subtree {
			struct wlr_scene_tree *tree;
			struct wlr_scene_buffer *top, *bottom, *left, *right;
		} subtrees[2]; /* indexed by enum ssd_active_state */
	} border;

	struct ssd_shadow_scene {
		struct wlr_scene_tree *tree;
		struct ssd_shadow_subtree {
			struct wlr_scene_tree *tree;
			struct wlr_scene_buffer *top, *bottom, *left, *right,
				*top_left, *top_right, *bottom_left, *bottom_right;
		} subtrees[2]; /* indexed by enum ssd_active_state */
	} shadow;

	/*
	 * Space between the extremities of the view's wlr_surface
	 * and the max extents of the server-side decorations.
	 * For xdg-shell views with CSD, this margin is zero.
	 */
	struct border margin;
};

struct ssd_button {
	struct wlr_scene_node *node;
	enum lab_node_type type;

	/*
	 * Bitmap of lab_button_state that represents a combination of
	 * hover/toggled/rounded states.
	 */
	uint8_t state_set;
	/*
	 * Image buffers for each combination of hover/toggled/rounded states.
	 * img_buffers[state_set] is displayed. Some of these can be NULL
	 * (e.g. img_buffers[LAB_BS_ROUNDED] is set only for corner buttons).
	 *
	 * When the button type is LAB_NODE_BUTTON_WINDOW_ICON,
	 * these are all NULL and window_icon is used instead.
	 */
	struct scaled_img_buffer *img_buffers[LAB_BS_ALL + 1];

	struct scaled_icon_buffer *window_icon;

	struct wl_list link; /* ssd_titlebar_subtree.buttons_{left,right} */
};

struct wlr_buffer;
struct wlr_scene_tree;

/* SSD internal helpers to create various SSD elements */
struct ssd_button *attach_ssd_button(struct wl_list *button_parts,
	enum lab_node_type type, struct wlr_scene_tree *parent,
	struct lab_img *imgs[LAB_BS_ALL + 1], int x, int y,
	struct view *view);

/* SSD internal */
void ssd_titlebar_create(struct ssd *ssd);
void ssd_titlebar_update(struct ssd *ssd);
void ssd_titlebar_destroy(struct ssd *ssd);
bool ssd_should_be_squared(struct ssd *ssd);

void ssd_border_create(struct ssd *ssd);
void ssd_border_update(struct ssd *ssd);
void ssd_border_destroy(struct ssd *ssd);

void ssd_extents_create(struct ssd *ssd);
void ssd_extents_update(struct ssd *ssd);
void ssd_extents_destroy(struct ssd *ssd);

void ssd_shadow_create(struct ssd *ssd);
void ssd_shadow_update(struct ssd *ssd);
void ssd_shadow_destroy(struct ssd *ssd);

#endif /* LABWC_SSD_INTERNAL_H */
