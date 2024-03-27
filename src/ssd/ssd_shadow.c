// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "common/scene-helpers.h"
#include "labwc.h"
#include "buffer.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"
#include <cairo.h>

#define FOR_EACH_STATE(ssd, tmp) FOR_EACH(tmp, \
	&(ssd)->shadow.active, \
	&(ssd)->shadow.inactive)

/*
 * Implements point_accepts_input for a buffer which never accepts input
 * because drop-shadows should never catch clicks!
 */
static bool
never_accepts_input(struct wlr_scene_buffer *buffer, double *sx, double *sy)
{
	return false;
}

static void
set_shadow_geometry(struct ssd *ssd)
{
	struct view *view = ssd->view;
	struct theme *theme = view->server->theme;
	int width = view->current.width;
	int height = view_effective_height(view, false) + ssd->titlebar.height;

	struct ssd_part *part;
	struct ssd_sub_tree *subtree;

	FOR_EACH_STATE(ssd, subtree) {
		bool active = subtree == &ssd->shadow.active;
		int visible_shadow_width = active ? theme->window_active_shadow_radius
			: theme->window_inactive_shadow_radius;
		/* inset as a proportion of shadow width */
		double inset_proportion = active ? theme->window_active_shadow_inset
			: theme->window_inactive_shadow_inset;
		/* inset in actual pixels */
		int inset = inset_proportion * (double)visible_shadow_width;
		int total_shadow_width = visible_shadow_width + inset;

		wl_list_for_each(part, &subtree->parts, link) {
			struct wlr_scene_buffer *scene_buf =
				wlr_scene_buffer_from_node(part->node);
			switch (part->type) {
			case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
				wlr_scene_node_set_position(part->node,
					width - inset,
					-ssd->titlebar.height + height - inset);
				continue;
			case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
				wlr_scene_node_set_position(part->node,
					-visible_shadow_width,
					-ssd->titlebar.height + height - inset);
				continue;
			case LAB_SSD_PART_CORNER_TOP_LEFT:
				wlr_scene_node_set_position(part->node,
					-visible_shadow_width,
					-ssd->titlebar.height - visible_shadow_width);
				continue;
			case LAB_SSD_PART_CORNER_TOP_RIGHT:
				wlr_scene_node_set_position(part->node,
					width - inset,
					-ssd->titlebar.height - visible_shadow_width);
				continue;
			case LAB_SSD_PART_RIGHT:
				wlr_scene_node_set_position(part->node,
					width - inset,
					-ssd->titlebar.height + inset);
				wlr_scene_buffer_set_dest_size(scene_buf,
					total_shadow_width, height - 2 * inset);
				continue;
			case LAB_SSD_PART_BOTTOM:
				wlr_scene_node_set_position(part->node, inset,
					-ssd->titlebar.height + height - inset);
				wlr_scene_buffer_set_dest_size(scene_buf,
					width - 2 * inset, total_shadow_width);
				continue;
			case LAB_SSD_PART_LEFT:
				wlr_scene_node_set_position(part->node,
					-visible_shadow_width,
					-ssd->titlebar.height + inset);
				wlr_scene_buffer_set_dest_size(scene_buf,
					total_shadow_width, height - 2 * inset);
				continue;
			case LAB_SSD_PART_TOP:
				wlr_scene_node_set_position(part->node, inset,
					-ssd->titlebar.height - visible_shadow_width);
				wlr_scene_buffer_set_dest_size(scene_buf,
					width - 2 * inset, total_shadow_width);
				continue;
			default:
				continue;
			}
		}
	} FOR_EACH_END
}

static void
make_shadow(struct wl_list *parts, enum ssd_part_type type,
	struct wlr_scene_tree *parent, struct wlr_buffer *buf,
	enum wl_output_transform tx)
{
	struct ssd_part *part = add_scene_buffer(
		parts, type, parent, buf, 0, 0);
	struct wlr_scene_buffer *scene_buf =
		wlr_scene_buffer_from_node(part->node);
	wlr_scene_buffer_set_transform(scene_buf, tx);
	scene_buf->point_accepts_input = never_accepts_input;
}

void
ssd_shadow_create(struct ssd *ssd)
{
	assert(ssd);
	assert(!ssd->shadow.tree);

	ssd->shadow.tree = wlr_scene_tree_create(ssd->tree);

	struct theme *theme = ssd->view->server->theme;
	struct wlr_buffer *corner_buffer;
	struct wlr_buffer *edge_buffer;
	struct ssd_sub_tree *subtree;
	struct wlr_scene_tree *parent;

	FOR_EACH_STATE(ssd, subtree) {
		subtree->tree = wlr_scene_tree_create(ssd->shadow.tree);
		parent = subtree->tree;
		if (subtree == &ssd->shadow.active) {
			corner_buffer = &theme->shadow_corner_active->base;
			edge_buffer = &theme->shadow_edge_active->base;
		} else {
			corner_buffer = &theme->shadow_corner_inactive->base;
			edge_buffer = &theme->shadow_edge_inactive->base;
		}
		wl_list_init(&subtree->parts);

		make_shadow(&subtree->parts,
			LAB_SSD_PART_CORNER_BOTTOM_RIGHT, parent,
			corner_buffer, WL_OUTPUT_TRANSFORM_NORMAL);
		make_shadow(&subtree->parts, LAB_SSD_PART_CORNER_BOTTOM_LEFT,
			parent, corner_buffer, WL_OUTPUT_TRANSFORM_90);
		make_shadow(&subtree->parts, LAB_SSD_PART_CORNER_TOP_LEFT,
			parent, corner_buffer, WL_OUTPUT_TRANSFORM_180);
		make_shadow(&subtree->parts, LAB_SSD_PART_CORNER_TOP_RIGHT,
			parent, corner_buffer, WL_OUTPUT_TRANSFORM_270);
		make_shadow(&subtree->parts, LAB_SSD_PART_RIGHT, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_NORMAL);
		make_shadow(&subtree->parts, LAB_SSD_PART_BOTTOM, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_90);
		make_shadow(&subtree->parts, LAB_SSD_PART_LEFT, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_180);
		make_shadow(&subtree->parts, LAB_SSD_PART_TOP, parent,
			edge_buffer, WL_OUTPUT_TRANSFORM_270);

	} FOR_EACH_END

	set_shadow_geometry(ssd);

	/* No drop-shadow on maximised windows */
	bool maximized = ssd->view->maximized == VIEW_AXIS_BOTH;
	wlr_scene_node_set_enabled(&ssd->shadow.tree->node, !maximized);
}

void
ssd_shadow_update(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->shadow.tree);

	struct view *view = ssd->view;
	bool maximized = view->maximized == VIEW_AXIS_BOTH;
	if (ssd->shadow.tree->node.enabled == maximized) {
		wlr_scene_node_set_enabled(
			&ssd->shadow.tree->node, !maximized);
	}
	if (!maximized) {
		set_shadow_geometry(ssd);
	}
}

void
ssd_shadow_destroy(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->shadow.tree);

	wlr_scene_node_destroy(&ssd->shadow.tree->node);
	ssd->shadow.tree = NULL;
}
