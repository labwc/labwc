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
		if (theme->beveled_border) {

			// These will otherwise get left under the window when we reload
					
			subtree->left = lab_wlr_scene_rect_create(parent, 1, 1, color);

	
			subtree->right = lab_wlr_scene_rect_create(parent, 1, 1, color);
	
			subtree->bottom = lab_wlr_scene_rect_create(parent, 1, 1, color);
	
			subtree->top = lab_wlr_scene_rect_create(parent, 1, 1, color);

		
					
			int bevelSize = theme->border_bevel_width; // TODO: configurable
			
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
			uint32_t *right_data = znew_n(uint32_t, theme->border_width);
			uint32_t *bottom_data = znew_n(uint32_t, theme->border_width);

			for (int i = 0; i < bw; i++) {
				if (i<bevelSize) {
					left_data[i] = col1;
					top_data[i] = col1;
					right_data[i] = col1;
					bottom_data[i] = col1;

				} else if (i > (bw-bevelSize-1)) {
					left_data[i] = col0;
					top_data[i] = col0;
					right_data[i] = col0;
					bottom_data[i] = col0;

				} else {
					left_data[i] = col;
					top_data[i] = col;
					right_data[i] = col;
					bottom_data[i] = col;					
				}
			}


			uint32_t *tl_data = znew_n(uint32_t, bw*bw);
			uint32_t *tr_data = znew_n(uint32_t, bw*bw);
			uint32_t *bl_data = znew_n(uint32_t, bw*bw);
			uint32_t *br_data = znew_n(uint32_t, bw*bw);
			// Fill with solid
			for (int i=0; i<bw;i++) {
				for (int j=0; j<bw;j++) {
					tl_data[PIXEL(i, j)] = col;
					tr_data[PIXEL(i, j)] = col;
					bl_data[PIXEL(i, j)] = col;
					br_data[PIXEL(i, j)] = col;
				}
			}

		
			// Main Corners
			for (int i=0; i < bevelSize; i++) {
			
				// Solid bar parts
				for (int j=0; j<bw; j++) {
					// Top left corner:  Entire "bevel size" top rows are highlighted
					tl_data[PIXEL(j, i)] = col1;
					// First "bevel size" top columns are highlighted
					tl_data[PIXEL(i, j)] = col1;
									
					// Bottom Right corner:  Entire "bevel size" last rows are lowlight
					br_data[PIXEL(j, (bw-1-i))] = col0;
					// Last "bevel size" columns are lowlight
					br_data[PIXEL((bw-1-i), j)] = col0;
				
				
					// Bottom left corner:  Entire "bevel size" last rows are lowlight
					bl_data[PIXEL(j, (bw-1-i))] = col0;
					// First "bevel size" columns are highlight, except for the bottom right corner
					bl_data[PIXEL(i, j)] = col1;
					
					// Top Right corner:  Entire "bevel size" first rows are highlight
					tr_data[PIXEL(j, i)] = col1;
					// Last "bevel size" columns are lowlight, except for the top left
					tr_data[PIXEL((bw-1-i), j)] = col0;					

				}
			}
			// Beveled Corner Parts
			for (int i=0; i < bevelSize; i++) {

				for (int j=0; j<bevelSize; j++) {
   		                // Outer Corners
					// Bottom left corner: 
					// First "bevel size" columns are highlight, except for the bottom right corner
					bl_data[PIXEL(i, (bw - 1 - j))] = (j >= i) ? col1 : col0;
					
					// Top Right corner:
					// Last "bevel size" columns are lowlight, except for the top left
					tr_data[PIXEL((bw-1-i), j)] = (j > i) ? col0 : col1;
					
					
				// Inner Corners
					// Top left corner:  Bottom right is all dark
					tl_data[PIXEL((bw-1-i), (bw - 1 - j))] = col0;
					
                                        // Bottom Right corner:  Top left is all light
					br_data[PIXEL(i, j)] = col1;
					
					// Top Right corner:
					// Interior bottom left is dark on top, light on bottom
					tr_data[PIXEL(i, (bw-1-j))] = (i > j) ? col1 : col0;						
					
					// Bottom Left corner:
					// Interior top right is dark on top, light on bottom
					bl_data[PIXEL((bw-1-i), j)] = (i > j) ? col0 : col1;						

											

				}
			
			
			}


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
		} else {			
		
			subtree->left = lab_wlr_scene_rect_create(parent,
				theme->border_width, height, color);
	
			subtree->right = lab_wlr_scene_rect_create(parent,
				theme->border_width, height, color);
	
			subtree->bottom = lab_wlr_scene_rect_create(parent,
				full_width, theme->border_width, color);
	
			subtree->top = lab_wlr_scene_rect_create(parent,
				MAX(width - 2 * corner_width, 0), theme->border_width, color);
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
