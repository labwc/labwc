// SPDX-License-Identifier: GPL-2.0-only
#include "common/lab-scene-rect.h"
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "common/macros.h"
#include "common/borderset.h"
#include "buffer.h"

struct border_scene {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *top, *bottom, *left, *right;
	struct wlr_scene_buffer *tlcorner, *trcorner, *blcorner, *brcorner,
			*ttexture, *ltexture, *rtexture, *btexture;
};

static void
handle_node_destroy(struct wl_listener *listener, void *data)
{
	struct lab_scene_rect *rect = wl_container_of(listener, rect, node_destroy);
	wl_list_remove(&rect->node_destroy.link);
	free(rect->borders);
	free(rect);
}

struct lab_scene_rect *
lab_scene_rect_create(struct wlr_scene_tree *parent,
		struct lab_scene_rect_options *opts)
{
	struct lab_scene_rect *rect = znew(*rect);
	rect->border_width = opts->border_width;
	rect->nr_borders = opts->nr_borders;
	rect->borders = znew_n(rect->borders[0], opts->nr_borders);
	rect->tree = lab_wlr_scene_tree_create(parent);

	if (opts->bg_color) {
		rect->fill = lab_wlr_scene_rect_create(rect->tree, 0, 0, opts->bg_color);
	}

	for (int i = 0; i < rect->nr_borders; i++) {
		struct border_scene *border = &rect->borders[i];
		float *color = opts->border_colors[i];
		border->tree = lab_wlr_scene_tree_create(rect->tree);
		// Beveled mode 0 = normal outline
		// Beveled mode 1 = full bevel with sharp internal corners
		if (opts->beveled > 0) {
		
			float r = color[0];
			float g = color[1];
			float b = color[2];
			float a = color[3];
			int bw = rect->border_width;
			uint32_t colour32 = (uint32_t)(a*255) << 24 | (uint32_t)(r*255) << 16 | (uint32_t)(g*255) << 8 | (uint32_t)(b*255);
			struct borderset * renderedborders = getBorders(colour32, bw, 1);








			
			
			
			
			border->top = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			border->right = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			border->bottom = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			border->left = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			
			struct lab_data_buffer *ttexture_buffer =
				buffer_create_from_data(renderedborders->top, 1, 1, 4);
			border->ttexture = wlr_scene_buffer_create(border->tree, &ttexture_buffer->base);
			
			struct lab_data_buffer *ltexture_buffer =
				buffer_create_from_data(renderedborders->left, 1, 1, 4);
			border->ltexture = wlr_scene_buffer_create(border->tree, &ltexture_buffer->base);
			
			struct lab_data_buffer *rtexture_buffer =
				buffer_create_from_data(renderedborders->right, 1, 1, 4);
			border->rtexture = wlr_scene_buffer_create(border->tree, &rtexture_buffer->base);
			
			struct lab_data_buffer *btexture_buffer =
				buffer_create_from_data(renderedborders->bottom, 1, 1, 4);
			border->btexture = wlr_scene_buffer_create(border->tree, &btexture_buffer->base);
						

			struct lab_data_buffer *tltexture_buffer =
				buffer_create_from_data(renderedborders->tl, bw, bw, 4*bw);
			border->tlcorner = wlr_scene_buffer_create(border->tree, &tltexture_buffer->base);


			struct lab_data_buffer *trtexture_buffer =
				buffer_create_from_data(renderedborders->tr, bw, bw, 4*bw);
			border->trcorner = wlr_scene_buffer_create(border->tree, &trtexture_buffer->base);


			struct lab_data_buffer *bltexture_buffer =
				buffer_create_from_data(renderedborders->bl, bw, bw, 4*bw);
			border->blcorner = wlr_scene_buffer_create(border->tree, &bltexture_buffer->base);

			struct lab_data_buffer *brtexture_buffer =
				buffer_create_from_data(renderedborders->br, bw, bw, 4*bw);
			border->brcorner = wlr_scene_buffer_create(border->tree, &brtexture_buffer->base);
		} else {
			border->top = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			border->right = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			border->bottom = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			border->left = lab_wlr_scene_rect_create(border->tree, 0, 0, color);
			border->tlcorner=NULL;
			border->trcorner=NULL;
			border->blcorner=NULL;
			border->brcorner=NULL;
		}
		
		
	}

	rect->node_destroy.notify = handle_node_destroy;
	wl_signal_add(&rect->tree->node.events.destroy, &rect->node_destroy);

	lab_scene_rect_set_size(rect, opts->width, opts->height);

	return rect;
}

static void
resize_border(struct border_scene *border, int border_width, int width, int height)
{
	/*
	 * The border is drawn like below:
	 *
	 * <--width-->
	 * +---------+   ^
	 * +-+-----+-+   |
	 * | |     | | height
	 * | |     | |   |
	 * +-+-----+-+   |
	 * +---------+   v
	 */

	if ((width < border_width * 2) || (height < border_width * 2)) {
		wlr_scene_node_set_enabled(&border->tree->node, false);
		return;
	}
	wlr_scene_node_set_enabled(&border->tree->node, true);

	wlr_scene_node_set_position(&border->top->node, 0, 0);
	wlr_scene_node_set_position(&border->bottom->node, 0, height - border_width);
	wlr_scene_node_set_position(&border->left->node, 0, border_width);
	wlr_scene_node_set_position(&border->right->node, width - border_width, border_width);

	wlr_scene_rect_set_size(border->top, width, border_width);
	wlr_scene_rect_set_size(border->bottom, width, border_width);
	wlr_scene_rect_set_size(border->left, border_width, height - border_width * 2);
	wlr_scene_rect_set_size(border->right, border_width, height - border_width * 2);
	
	if (border->tlcorner != NULL) {
		wlr_scene_buffer_set_dest_size(border->ttexture,
				width, border_width);		
		wlr_scene_node_set_position(&border->ttexture->node,
				0,0);	
				
		wlr_scene_buffer_set_dest_size(border->btexture,
				width, border_width);		
		wlr_scene_node_set_position(&border->btexture->node,
				 0, height - border_width);
				

		wlr_scene_buffer_set_dest_size(border->ltexture,
				border_width, height - border_width * 2);
		wlr_scene_node_set_position(&border->ltexture->node,
				0, border_width);	
				
		wlr_scene_buffer_set_dest_size(border->rtexture,
				border_width, height - border_width * 2);
		wlr_scene_node_set_position(&border->rtexture->node,
				width - border_width, border_width);	
				
				
				
	
	
		wlr_scene_buffer_set_dest_size(border->tlcorner,
				border_width, border_width);		
			wlr_scene_node_set_position(&border->tlcorner->node,
				0,0);	

			wlr_scene_buffer_set_dest_size(border->trcorner,
				border_width, border_width);		
			wlr_scene_node_set_position(&border->trcorner->node,
				width-border_width, 0);	


			wlr_scene_buffer_set_dest_size(border->brcorner,
				border_width, border_width);		
			wlr_scene_node_set_position(&border->brcorner->node,
				width-border_width , height-border_width);	


			wlr_scene_buffer_set_dest_size(border->blcorner,
				border_width, border_width);		
			wlr_scene_node_set_position(&border->blcorner->node,
				0, height-border_width);	
	
	
	}
}

void
lab_scene_rect_set_size(struct lab_scene_rect *rect, int width, int height)
{
	assert(rect);
	int border_width = rect->border_width;

	for (int i = 0; i < rect->nr_borders; i++) {
		struct border_scene *border = &rect->borders[i];
		resize_border(border, border_width,
			width - 2 * border_width * i,
			height - 2 * border_width * i);
		wlr_scene_node_set_position(&border->tree->node,
			i * border_width, i * border_width);
	}

	if (rect->fill) {
		wlr_scene_rect_set_size(rect->fill, width, height);
	}
}
