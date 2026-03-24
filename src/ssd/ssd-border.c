// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "ssd.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

void
ssd_border_create(struct ssd *ssd)
{
	assert(ssd);
	assert(!ssd->border.tree);

	struct view *view = ssd->view;
	struct theme *theme = rc.theme;
	int bw = theme->border_width;
	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
	int full_width = width + 2 * theme->border_width;
	int corner_width = ssd_get_corner_width();

	ssd->border.tree = lab_wlr_scene_tree_create(ssd->tree);
	wlr_scene_node_set_position(&ssd->border.tree->node, -theme->border_width, 0);

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_border_subtree *subtree = &ssd->border.subtrees[active];
		subtree->tree = lab_wlr_scene_tree_create(ssd->border.tree);
		struct wlr_scene_tree *parent = subtree->tree;
		wlr_scene_node_set_enabled(&parent->node, active);
		float *color = theme->window[active].border_color;


		subtree->left = lab_wlr_scene_rect_create(parent,
			theme->border_width, height, color);
	
		subtree->right = lab_wlr_scene_rect_create(parent,
			theme->border_width, height, color);
	
		subtree->bottom = lab_wlr_scene_rect_create(parent,
			full_width, theme->border_width, color);
	
		subtree->top = lab_wlr_scene_rect_create(parent,
			MAX(width - 2 * corner_width, 0), theme->border_width, color);



		if (theme->beveled_border) {
			/* From Pull request 3382 */
			uint8_t r = color[0] * 255;
			uint8_t g = color[1] * 255;
			uint8_t b = color[2] * 255;
			uint8_t a = color[3] * 255;

			/* highlight */
			uint8_t r1 = MIN(r * 5 / 4, a);
			uint8_t g1 = MIN(g * 5 / 4, a);
			uint8_t b1 = MIN(b * 5 / 4, a);

			/* darker outline */
			uint8_t r0 = r / 2;
			uint8_t g0 = g / 2;
			uint8_t b0 = b / 2;

			uint32_t col = ((uint32_t)a << 24) | ((uint32_t)r << 16)
				| ((uint32_t)g << 8) | b;
			uint32_t col0 = ((uint32_t)a << 24) | ((uint32_t)r0 << 16)
				| ((uint32_t)g0 << 8) | b0;
			uint32_t col1 = ((uint32_t)a << 24) | ((uint32_t)r1 << 16)
				| ((uint32_t)g1 << 8) | b1;


			uint32_t *left_data = znew_n(uint32_t, bw);
			uint32_t *top_data = znew_n(uint32_t, bw);
			left_data[0] = top_data[0] = col1;
			left_data[1] = top_data[1] = col1;
			for (int i = 2; i < bw; i++) {
				left_data[i] = col;
				top_data[i] = col;
			}
			left_data[bw-1] = top_data[bw-1] = col0;
			left_data[bw-2] = top_data[bw-2] = col0;



			uint32_t *tl_data = znew_n(uint32_t, bw*bw);
			uint32_t *tr_data = znew_n(uint32_t, bw*bw);
			uint32_t *bl_data = znew_n(uint32_t, bw*bw);
			uint32_t *br_data = znew_n(uint32_t, bw*bw);
			for (int i=0; i<bw;i++) {
				for (int j=0; j<bw;j++) {
					tl_data[i*bw + j] = col;
					tr_data[i*bw + j] = col;
					bl_data[i*bw + j] = col;
					br_data[i*bw + j] = col;
					if (i<2) {
						tl_data[i*bw + j] = col1;
						tr_data[i*bw + j] = col1;
					}
					if (i>bw -3) {
						bl_data[i*bw + j] = col0;
						br_data[i*bw + j] = col0;
					} 
					if (j<2) {
						tl_data[i*bw + j] = col1;
						bl_data[i*bw + j] = col1;
					}
					if (j>bw -3) {
						tr_data[i*bw + j] = col0;
						br_data[i*bw + j] = col0;
					}
				}
			}
			tl_data[bw*bw -1] = col0;
			tl_data[bw*bw -2] = col0;
			tl_data[(bw-1)*bw -1] = col0;
			tl_data[(bw-1)*bw -2] = col0;
			tr_data[(bw-1)*bw + (0)] = col0;		
			tr_data[(bw-1)*bw + (1)] = col0;
			tr_data[(bw-2)*bw + (0)] = col0;		
			tr_data[(bw-2)*bw + (1)] = col0;				
			bl_data[(0)*bw + (bw-1)] = col1;
			bl_data[(0)*bw + (bw-2)] = col1;
			bl_data[(1)*bw + (bw-1)] = col1;
			bl_data[(1)*bw + (bw-2)] = col1;
			br_data[(0)*bw + (0)] = col1;
			br_data[(0)*bw + (1)] = col1;
			br_data[(1)*bw + (0)] = col1;
			br_data[(1)*bw + (1)] = col1;


			/* bottom and right are identical */
			uint32_t *right_data = znew_n(uint32_t, theme->border_width);
			uint32_t *bottom_data = znew_n(uint32_t, theme->border_width);
			right_data[0]=bottom_data[0]=col1;
			right_data[1]=bottom_data[1]=col1;
			for (int i = 2; i < theme->border_width - 1; i++) {
				right_data[i] = col;
				bottom_data[i] = col;
			}
			right_data[theme->border_width - 1] = bottom_data[theme->border_width - 1] = col0;
			right_data[theme->border_width - 2] = bottom_data[theme->border_width - 2] = col0;


			struct lab_data_buffer *ttexture_buffer =
				buffer_create_from_data(top_data, 1,theme->border_width,
					4);
			subtree->ttexture = wlr_scene_buffer_create(parent, &ttexture_buffer->base);
			wlr_buffer_drop(&ttexture_buffer->base);

			struct lab_data_buffer *btexture_buffer =
				buffer_create_from_data(bottom_data, 1,theme->border_width,
					4);
			subtree->btexture = wlr_scene_buffer_create(parent, &btexture_buffer->base);
			wlr_buffer_drop(&btexture_buffer->base);	

			struct lab_data_buffer *ltexture_buffer =
				buffer_create_from_data(left_data, theme->border_width, 1,
					4*theme->border_width);
			subtree->ltexture = wlr_scene_buffer_create(parent, &ltexture_buffer->base);
			wlr_buffer_drop(&ltexture_buffer->base);
				
			struct lab_data_buffer *rtexture_buffer =
				buffer_create_from_data(right_data, theme->border_width, 1, 
					4*theme->border_width);
			subtree->rtexture = wlr_scene_buffer_create(parent, &rtexture_buffer->base);
			wlr_buffer_drop(&rtexture_buffer->base);	


			struct lab_data_buffer *tltexture_buffer =
				buffer_create_from_data(tl_data, theme->border_width, theme->border_width, 
					4*theme->border_width);
			subtree->tlcorner = wlr_scene_buffer_create(parent, &tltexture_buffer->base);
			wlr_buffer_drop(&tltexture_buffer->base);

			struct lab_data_buffer *trtexture_buffer =
				buffer_create_from_data(tr_data, theme->border_width, theme->border_width, 
					4*theme->border_width);
			subtree->trcorner = wlr_scene_buffer_create(parent, &trtexture_buffer->base);
			wlr_buffer_drop(&trtexture_buffer->base);	


			struct lab_data_buffer *bltexture_buffer =
				buffer_create_from_data(bl_data, theme->border_width, theme->border_width, 
					4*theme->border_width);
			subtree->blcorner = wlr_scene_buffer_create(parent, &bltexture_buffer->base);
			wlr_buffer_drop(&bltexture_buffer->base);

			struct lab_data_buffer *brtexture_buffer =
				buffer_create_from_data(br_data, theme->border_width, theme->border_width, 
					4*theme->border_width);
			subtree->brcorner = wlr_scene_buffer_create(parent, &brtexture_buffer->base);
			wlr_buffer_drop(&brtexture_buffer->base);	
		}


	
		if (!theme->beveled_border) {			
			wlr_scene_node_set_position(&subtree->left->node, 0, 0);
			wlr_scene_node_set_position(&subtree->right->node,
				theme->border_width + width, 0);
			wlr_scene_node_set_position(&subtree->bottom->node,
				0, height);
		
			wlr_scene_node_set_position(&subtree->top->node,
				theme->border_width + corner_width,
				-(ssd->titlebar.height + theme->border_width));
		}
			
	}
	
	
	

	if (view->maximized == VIEW_AXIS_BOTH) {
		wlr_scene_node_set_enabled(&ssd->border.tree->node, false);
	}

	if (view->current.width > 0 && view->current.height > 0) {
		/*
		 * The SSD is recreated by a Reconfigure request
		 * thus we may need to handle squared corners.
		 */
		ssd_border_update(ssd);
	}
}

void
ssd_border_update(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->border.tree);

	struct view *view = ssd->view;
	if (view->maximized == VIEW_AXIS_BOTH
			&& ssd->border.tree->node.enabled) {
		/* Disable borders on maximize */
		wlr_scene_node_set_enabled(&ssd->border.tree->node, false);
		ssd->margin = ssd_thickness(ssd->view);
	}

	if (view->maximized == VIEW_AXIS_BOTH) {
		return;
	} else if (!ssd->border.tree->node.enabled) {
		/* And re-enabled them when unmaximized */
		wlr_scene_node_set_enabled(&ssd->border.tree->node, true);
		ssd->margin = ssd_thickness(ssd->view);
	}

	struct theme *theme = rc.theme;

	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
	int full_width = width + 2 * theme->border_width;
	int corner_width = ssd_get_corner_width();

	/*
	 * From here on we have to cover the following border scenarios:
	 * Non-tiled (partial border, rounded corners):
	 *    _____________
	 *   o           oox
	 *  |---------------|
	 *  |_______________|
	 *
	 * Tiled (full border, squared corners):
	 *   _______________
	 *  |o           oox|
	 *  |---------------|
	 *  |_______________|
	 *
	 * Tiled or non-tiled with zero title height (full boarder, no title):
	 *   _______________
	 *  |_______________|
	 */

	int side_height = ssd->state.was_squared
		? height + ssd->titlebar.height
		: height;
	int side_y = ssd->state.was_squared
		? -ssd->titlebar.height
		: 0;
	int top_width = ssd->titlebar.height <= 0 || ssd->state.was_squared
		? full_width
		: MAX(width - 2 * corner_width, 0);
	int top_x = ssd->titlebar.height <= 0 || ssd->state.was_squared
		? 0
		: theme->border_width + corner_width;

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_border_subtree *subtree = &ssd->border.subtrees[active];
		
		
		if (theme->beveled_border) {	
			wlr_scene_buffer_set_dest_size(subtree->ttexture,
				full_width, theme->border_width);		
			wlr_scene_node_set_position(&subtree->ttexture->node,
				0, -(ssd->titlebar.height + theme->border_width));

			wlr_scene_buffer_set_dest_size(subtree->rtexture,
				theme->border_width, side_height+(ssd->titlebar.height + theme->border_width));
			wlr_scene_node_set_position(&subtree->rtexture->node,
				theme->border_width + width, -(ssd->titlebar.height + theme->border_width));

			wlr_scene_buffer_set_dest_size(subtree->btexture,
				full_width, theme->border_width);		
			wlr_scene_node_set_position(&subtree->btexture->node,
				0, height);

			wlr_scene_buffer_set_dest_size(subtree->ltexture,
				theme->border_width, side_height+(ssd->titlebar.height + theme->border_width));
			wlr_scene_node_set_position(&subtree->ltexture->node,
				0, -(ssd->titlebar.height + theme->border_width));

			wlr_scene_buffer_set_dest_size(subtree->tlcorner,
				theme->border_width, theme->border_width);		
			wlr_scene_node_set_position(&subtree->tlcorner->node,
				0, -(ssd->titlebar.height + theme->border_width));	

			wlr_scene_buffer_set_dest_size(subtree->trcorner,
				theme->border_width, theme->border_width);		
			wlr_scene_node_set_position(&subtree->trcorner->node,
				theme->border_width + width, -(ssd->titlebar.height + theme->border_width));	


			wlr_scene_buffer_set_dest_size(subtree->brcorner,
				theme->border_width, theme->border_width);		
			wlr_scene_node_set_position(&subtree->brcorner->node,
				theme->border_width + width, height);	


			wlr_scene_buffer_set_dest_size(subtree->blcorner,
				theme->border_width, theme->border_width);		
			wlr_scene_node_set_position(&subtree->blcorner->node,
				0, height);	
		} else {
			wlr_scene_rect_set_size(subtree->left,
				theme->border_width, side_height);
			wlr_scene_node_set_position(&subtree->left->node,
				0, side_y);		
			wlr_scene_rect_set_size(subtree->right,
				theme->border_width, side_height);
			wlr_scene_node_set_position(&subtree->right->node,
				theme->border_width + width, side_y);
			wlr_scene_rect_set_size(subtree->bottom,
				full_width, theme->border_width);
			wlr_scene_node_set_position(&subtree->bottom->node,
				0, height);
			wlr_scene_rect_set_size(subtree->top,
				top_width, theme->border_width);
			wlr_scene_node_set_position(&subtree->top->node,
				top_x, -(ssd->titlebar.height + theme->border_width));
		}
	}
}

void
ssd_border_destroy(struct ssd *ssd)
{
	assert(ssd);
	assert(ssd->border.tree);

	wlr_scene_node_destroy(&ssd->border.tree->node);
	ssd->border = (struct ssd_border_scene){0};
}
