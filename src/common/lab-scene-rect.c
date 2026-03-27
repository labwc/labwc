// SPDX-License-Identifier: GPL-2.0-only
#include "common/lab-scene-rect.h"
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "common/macros.h"
#include "buffer.h"
#include "config/rcxml.h"
#include "theme.h"

struct border_scene {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *top, *bottom, *left, *right;
	struct wlr_scene_buffer *tlcorner, *trcorner, *blcorner, *brcorner;
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
		// Beveled mode 2 = "light" bevel without sharp corners.
		// This mode doesn't use the extra buffers.  It seems like when we render the window switcher
		// it wants to render the highlight buffers "immediately" and pollute the screen, but the sides
		// work normally
		if (opts->beveled > 0) {
			/* From Pull request 3382 */
			
			int bw = rect->border_width;
			
			// Floats for the rect versions.
			float r = color[0];
			float g = color[1];
			float b = color[2];
			float a = color[3];
			

			float r1 = r * 5 / 4;
			if (r1 > a) r1=a;
			float g1 = g * 5 / 4;
			if (g1 > a) g1=a;
			float b1 = b * 5 / 4;
			if (b1 > a) b1=a;

			/* darker outline */
			float r0 = r / 2;
			float g0 = g / 2;
			float b0 = b / 2;



			// Buffers are AARRGGBB 32-bit packed int
			uint32_t ll32 = ((uint32_t)(255*a) << 24) | ((uint32_t)(255*r0) << 16)
				| ((uint32_t)(255*g0) << 8) | (uint32_t)(255*b0);
			uint32_t hl32 = ((uint32_t)(255*a) << 24) | ((uint32_t)(255*r1) << 16)
				| ((uint32_t)(255*g1) << 8) | (uint32_t)(255*b1);


			const float highlight[4] = {r1, g1, b1, a};
			const float lowlight[4] = {r0, g0, b0, a};
			
			
			
			
			
			border->top = lab_wlr_scene_rect_create(border->tree, 0, 0, highlight);
			border->right = lab_wlr_scene_rect_create(border->tree, 0, 0, lowlight);
			border->bottom = lab_wlr_scene_rect_create(border->tree, 0, 0, lowlight);
			border->left = lab_wlr_scene_rect_create(border->tree, 0, 0, highlight);
			
			
			
			uint32_t *tl_data = znew_n(uint32_t, bw*bw);
			uint32_t *tr_data = znew_n(uint32_t, bw*bw);
			uint32_t *bl_data = znew_n(uint32_t, bw*bw);
			uint32_t *br_data = znew_n(uint32_t, bw*bw);
			

			// Fill with solid
			for (int j=0; j<bw;j++) {
				for (int k=0; k<bw;k++) {
					tl_data[PIXEL(j, k)] = hl32;
					tr_data[PIXEL(bw - 1 - j, k)] = (j > k) ? hl32 : ll32;
					bl_data[PIXEL(bw - 1 -j, k)] = (j > k) ? hl32 : ll32;
					br_data[PIXEL(j, k)] = ll32;
				}
			}
			
			if (opts->beveled == 1) {
				struct lab_data_buffer *tltexture_buffer =
					buffer_create_from_data(tl_data, bw, bw, 4*bw);
				border->tlcorner = wlr_scene_buffer_create(parent, &tltexture_buffer->base);
				wlr_buffer_drop(&tltexture_buffer->base);


				struct lab_data_buffer *trtexture_buffer =
					buffer_create_from_data(tr_data, bw, bw, 4*bw);
				border->trcorner = wlr_scene_buffer_create(parent, &trtexture_buffer->base);
				wlr_buffer_drop(&trtexture_buffer->base);	


				struct lab_data_buffer *bltexture_buffer =
					buffer_create_from_data(bl_data, bw, bw, 4*bw);
				border->blcorner = wlr_scene_buffer_create(parent, &bltexture_buffer->base);
				wlr_buffer_drop(&bltexture_buffer->base);

				struct lab_data_buffer *brtexture_buffer =
					buffer_create_from_data(br_data, bw, bw, 4*bw);
				border->brcorner = wlr_scene_buffer_create(parent, &brtexture_buffer->base);
				wlr_buffer_drop(&brtexture_buffer->base);	
			} else {
				border->tlcorner=NULL;
				border->trcorner=NULL;
				border->blcorner=NULL;
				border->brcorner=NULL;
			}
			
			
			
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
