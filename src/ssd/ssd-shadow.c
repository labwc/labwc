// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "ssd.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

/*
 * Implements point_accepts_input for a buffer which never accepts input
 * because drop-shadows should never catch clicks!
 */
static bool
never_accepts_input(struct wlr_scene_buffer *buffer, double *sx, double *sy)
{
	return false;
}

/*
 * Setup transform and scale for shadow corner buffers. Cropping is applied if
 * the window is short or narrow enough that corners would overlap, the amount
 * to crop is controlled by vertical_overlap and horizontal_overlap.
 */
static void
corner_scale_crop(struct wlr_scene_buffer *buffer, int horizontal_overlap,
		int vertical_overlap, int corner_size)
{
	int width = MAX(corner_size - horizontal_overlap, 0);
	int height = MAX(corner_size - vertical_overlap, 0);
	struct wlr_fbox src_box = {
		.x = horizontal_overlap,
		.y = vertical_overlap,
		.width = width,
		.height = height,
	};
	wlr_scene_buffer_set_source_box(buffer, &src_box);
	wlr_scene_buffer_set_dest_size(buffer, width, height);
}

/*
 * Set the position, scaling, and visibility for a single part of a window
 * drop-shadow.
 */
static void
set_shadow_parts_geometry(struct ssd_shadow_subtree *subtree,
		int width, int height, int titlebar_height, int corner_size,
		int inset, int visible_shadow_width)
{
	/*
	 * If the shadow inset is greater than half the overall window height
	 * or width (eg. because the window is shaded or because we have a
	 * small window with massive shadows) then the corners would overlap
	 * which looks horrible.  To avoid this, when the window is too narrow
	 * or short we hide the edges on that axis and clip off the portion of
	 * the corners which would overlap.  This does produce slight
	 * aberrations in the shadow shape where corners meet but it's not too
	 * noticeable.
	 */
	bool show_topbottom = width > inset * 2;
	bool show_sides = height > inset * 2;
	/* These values are the overlap on each corner (half total overlap) */
	int horizontal_overlap = MAX(0, inset - width / 2);
	int vertical_overlap = MAX(0, inset - height / 2);

	/*
	 * If window width or height is odd then making the corners equally
	 * sized when the edge is hidden would leave a single pixel gap
	 * between the corners. Showing a single pixel edge between clipped
	 * corners looks bad because the edge-piece doesn't match up with the
	 * corners after the corners are clipped. So fill the gap by making
	 * the top-left and bottom-right corners one pixel wider (if the width
	 * is odd) or taller (if the height is odd).
	 */
	int horizontal_overlap_downsized = horizontal_overlap;
	if (horizontal_overlap > 0) {
		horizontal_overlap_downsized -= width % 2;
	}
	int vertical_overlap_downsized = vertical_overlap;
	if (vertical_overlap > 0) {
		vertical_overlap_downsized -= height % 2;
	}

	int x;
	int y;

	x = width - inset + horizontal_overlap_downsized;
	y = -titlebar_height + height - inset + vertical_overlap_downsized;
	wlr_scene_node_set_position(&subtree->bottom_right->node, x, y);
	corner_scale_crop(subtree->bottom_right, horizontal_overlap_downsized,
		vertical_overlap_downsized, corner_size);

	x = -visible_shadow_width;
	y = -titlebar_height + height - inset + vertical_overlap;
	wlr_scene_node_set_position(&subtree->bottom_left->node, x, y);
	corner_scale_crop(subtree->bottom_left, horizontal_overlap,
		vertical_overlap, corner_size);

	x = -visible_shadow_width;
	y = -titlebar_height - visible_shadow_width;
	wlr_scene_node_set_position(&subtree->top_left->node, x, y);
	corner_scale_crop(subtree->top_left, horizontal_overlap_downsized,
		vertical_overlap_downsized, corner_size);

	x = width - inset + horizontal_overlap;
	y = -titlebar_height - visible_shadow_width;
	wlr_scene_node_set_position(&subtree->top_right->node, x, y);
	corner_scale_crop(subtree->top_right, horizontal_overlap,
		vertical_overlap, corner_size);

	x = width;
	y = -titlebar_height + inset;
	wlr_scene_node_set_position(&subtree->right->node, x, y);
	wlr_scene_buffer_set_dest_size(subtree->right,
		visible_shadow_width, MAX(height - 2 * inset, 0));
	wlr_scene_node_set_enabled(&subtree->right->node, show_sides);

	x = inset;
	y = -titlebar_height + height;
	wlr_scene_node_set_position(&subtree->bottom->node, x, y);
	wlr_scene_buffer_set_dest_size(subtree->bottom,
		MAX(width - 2 * inset, 0), visible_shadow_width);
	wlr_scene_node_set_enabled(&subtree->bottom->node, show_topbottom);

	x = -visible_shadow_width;
	y = -titlebar_height + inset;
	wlr_scene_node_set_position(&subtree->left->node, x, y);
	wlr_scene_buffer_set_dest_size(subtree->left,
		visible_shadow_width, MAX(height - 2 * inset, 0));
	wlr_scene_node_set_enabled(&subtree->left->node, show_sides);

	x = inset;
	y = -titlebar_height - visible_shadow_width;
	wlr_scene_node_set_position(&subtree->top->node, x, y);
	wlr_scene_buffer_set_dest_size(subtree->top,
		MAX(width - 2 * inset, 0), visible_shadow_width);
	wlr_scene_node_set_enabled(&subtree->top->node, show_topbottom);
}

static void
set_shadow_geometry(struct ssd *ssd)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int titlebar_height = ssd->titlebar.height;
	int width = view->current.width;
	int height = view_effective_height(view, false) + titlebar_height;

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_shadow_subtree *subtree = &ssd->shadow.subtrees[active];
		if (!subtree->tree) {
			/* Looks like this type of shadow is disabled */
			continue;
		}

		int visible_shadow_width = theme->window[active].shadow_size;
		/* inset as a proportion of shadow width */
		double inset_proportion = SSD_SHADOW_INSET;
		/* inset in actual pixels */
		int inset = inset_proportion * (double)visible_shadow_width;

		/*
		 * Total size of corner buffers including inset and visible
		 * portion.  Top and bottom are the same size (only the cutout
		 * is different).  The buffers are square so width == height.
		 */
		int corner_size =
			theme->window[active].shadow_corner_top->logical_height;

		set_shadow_parts_geometry(subtree, width, height,
			titlebar_height, corner_size, inset,
			visible_shadow_width);
	}
}

static struct wlr_scene_buffer *
make_shadow(struct view *view,
	struct wlr_scene_tree *parent, struct wlr_buffer *buf,
	enum wl_output_transform tx)
{
	struct wlr_scene_buffer *scene_buf =
		wlr_scene_buffer_create(parent, buf);
	wlr_scene_buffer_set_transform(scene_buf, tx);
	scene_buf->point_accepts_input = never_accepts_input;
	/*
	 * Pixman has odd behaviour with bilinear filtering on buffers only one
	 * pixel wide/tall. Use nearest-neighbour scaling to workaround.
	 */
	scene_buf->filter_mode = WLR_SCALE_FILTER_NEAREST;
	return scene_buf;
}

void
ssd_shadow_create(struct ssd *ssd)
{
	assert(ssd);
	assert(!ssd->shadow.tree);

	ssd->shadow.tree = wlr_scene_tree_create(ssd->tree);

	struct theme *theme = ssd->view->server->theme;
	struct view *view = ssd->view;

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_shadow_subtree *subtree = &ssd->shadow.subtrees[active];

		if (!rc.shadows_enabled) {
			/* Shadows are globally disabled */
			continue;
		}
		if (theme->window[active].shadow_size == 0) {
			/* Window shadows are disabled */
			continue;
		}

		subtree->tree = wlr_scene_tree_create(ssd->shadow.tree);
		struct wlr_scene_tree *parent = subtree->tree;
		struct wlr_buffer *corner_top_buffer =
			&theme->window[active].shadow_corner_top->base;
		struct wlr_buffer *corner_bottom_buffer =
			&theme->window[active].shadow_corner_bottom->base;
		struct wlr_buffer *edge_buffer =
			&theme->window[active].shadow_edge->base;

		subtree->bottom_right = make_shadow(view, parent,
			corner_bottom_buffer, WL_OUTPUT_TRANSFORM_NORMAL);
		subtree->bottom_left = make_shadow(view, parent,
			corner_bottom_buffer, WL_OUTPUT_TRANSFORM_FLIPPED);
		subtree->top_left = make_shadow(view, parent,
			corner_top_buffer, WL_OUTPUT_TRANSFORM_180);
		subtree->top_right = make_shadow(view, parent,
			corner_top_buffer, WL_OUTPUT_TRANSFORM_FLIPPED_180);
		subtree->right = make_shadow(view, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_NORMAL);
		subtree->bottom = make_shadow(view, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_90);
		subtree->left = make_shadow(view, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_180);
		subtree->top = make_shadow(view, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_270);
	}

	ssd_shadow_update(ssd);
}

void
ssd_shadow_update(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->shadow.tree);

	struct view *view = ssd->view;
	struct theme *theme = ssd->view->server->theme;
	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	bool tiled_shadows = false;
	if (rc.shadows_on_tiled) {
		if (rc.gap >= theme->window[SSD_ACTIVE].shadow_size
				&& rc.gap >= theme->window[SSD_INACTIVE].shadow_size) {
			tiled_shadows = true;
		} else {
			wlr_log(WLR_INFO, "gap size < shadow_size, ignore rc.shadows_ontiled");
		}
	};
	bool show_shadows = rc.shadows_enabled && !maximized
		&& (!view_is_tiled(ssd->view) || tiled_shadows);
	wlr_scene_node_set_enabled(&ssd->shadow.tree->node, show_shadows);
	if (show_shadows) {
		set_shadow_geometry(ssd);
	}
}

void
ssd_shadow_destroy(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->shadow.tree);

	wlr_scene_node_destroy(&ssd->shadow.tree->node);
	ssd->shadow = (struct ssd_shadow_scene){0};
}
