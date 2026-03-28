#include <wlr/types/wlr_scene.h>
#include "common/borderset.h"
#include "common/mem.h"
#include "common/macros.h"
#include "buffer.h"

struct borderset * getBorders(uint32_t id, int size, enum border_type type, int bevelSize) {
	struct borderset * current = borderCache;
	struct borderset * last;
	while (current != NULL) {
		if (current->size == size && current->id == id && current->type == type) {
			return current;
		}
		last = current;	
		current = current -> next;
	}
	// Fall through, we need to create a buffer.
	
	if (borderCache == NULL) {
		borderCache = createBuffer(id, size, type, bevelSize);
		return borderCache;
	} else {
		last->next = createBuffer(id, size, type, bevelSize);
		return last->next;
	}
	return NULL;
}

struct borderset * createBuffer(uint32_t id, int size, 	enum border_type type, int bevelSize) {
	struct borderset *newBorderset = znew(*newBorderset);
	
	newBorderset->next = NULL;
	newBorderset->id = id;
	newBorderset->size = size;
	newBorderset->type = type;
	
	
	// Use ID as a AARRGGBB colour
	uint8_t a = id >> 24 & 255;
	uint8_t r = id >> 16 & 255;
	uint8_t g = id >> 8 & 255;
	uint8_t b = id & 255;
	
	uint32_t r1 = r * 5 / 4;
		if (r1 > a) r1=a;
	uint32_t g1 = g * 5 / 4;
		if (g1 > a) g1=a;
	uint32_t b1 = b * 5 / 4;
		if (b1 > a) b1=a;

			/* darker outline */
	uint32_t r0 = r / 2;
	uint32_t g0 = g / 2;
	uint32_t b0 = b / 2;
	
	uint32_t hl32 = ((uint32_t)a << 24) | ((uint32_t)r1 << 16)
				| ((uint32_t)g1 << 8) | (uint32_t)b1;
	uint32_t ll32 = ((uint32_t)a << 24) | ((uint32_t)r0 << 16)
				| ((uint32_t)g0 << 8) | (uint32_t)b0;
	

	// All borders have NxN corners
	newBorderset->tl = znew_n(uint32_t, size*size);
	newBorderset->tr = znew_n(uint32_t, size*size);
	newBorderset->bl = znew_n(uint32_t, size*size);
	newBorderset->br = znew_n(uint32_t, size*size);
			
	switch(type) {
		case BORDER_SINGLE:  // Single bevel borders have 1x1 sides
			newBorderset->top = znew(uint32_t);
			newBorderset->left = znew(uint32_t);
			newBorderset->right = znew(uint32_t);		
			newBorderset->bottom = znew(uint32_t);
			*newBorderset->top = hl32;
			*newBorderset->left = hl32;
			*newBorderset->right = ll32;
			*newBorderset->bottom = ll32;
			
			// Fill with solid
			for (int j=0; j<size;j++) {
				for (int k=0; k<size;k++) {
					newBorderset->tl[PIXEL(j, k, size)] = hl32;
					newBorderset->tr[PIXEL(size - 1 - j, k, size)] = (j > k) ? hl32 : ll32;
					newBorderset->bl[PIXEL(size - 1 -j, k, size)] = (j > k) ? hl32 : ll32;
					newBorderset->br[PIXEL(j, k, size)] = ll32;
				}
			}
		
		
		
		break;


		case BORDER_DOUBLE:
			newBorderset->top = znew_n(uint32_t, size);
			newBorderset->left = znew_n(uint32_t, size);
			newBorderset->right = znew_n(uint32_t, size);
			newBorderset->bottom = znew_n(uint32_t, size);

			for (int i = 0; i < size; i++) {
				if (i<bevelSize) {
					newBorderset->left[i] = hl32;
					newBorderset->top[i] = hl32;
					newBorderset->right[i] = hl32;
					newBorderset->bottom[i] = hl32;

				} else if (i > (size-bevelSize-1)) {
					newBorderset->left[i] = ll32;
					newBorderset->top[i] = ll32;
					newBorderset->right[i] = ll32;
					newBorderset->bottom[i] = ll32;

				} else {
					newBorderset->left[i] = id;
					newBorderset->top[i] = id;
					newBorderset->right[i] = id;
					newBorderset->bottom[i] = id;					
				}
			}

			// Blank corners...
			for (int i=0; i<size;i++) {
				for (int j=0; j<size;j++) {
					newBorderset->tl[PIXEL(i, j, size)] = id;
					newBorderset->tr[PIXEL(i, j, size)] = id;
					newBorderset->bl[PIXEL(i, j, size)] = id;
					newBorderset->br[PIXEL(i, j, size)] = id;
				}
			}

		
			// Main Corners
			for (int i=0; i < bevelSize; i++) {
			
				// Solid bar parts
				for (int j=0; j<size; j++) {
					// Top left corner:  Entire "bevel size" top rows are highlighted
					newBorderset->tl[PIXEL(j, i, size)] = hl32;
					// First "bevel size" top columns are highlighted
					newBorderset->tl[PIXEL(i, j, size)] = hl32;
									
					// Bottom Right corner:  Entire "bevel size" last rows are lowlight
					newBorderset->br[PIXEL(j, (size-1-i), size)] = ll32;
					// Last "bevel size" columns are lowlight
					newBorderset->br[PIXEL((size-1-i), j, size)] = ll32;
				
				
					// Bottom left corner:  Entire "bevel size" last rows are lowlight
					newBorderset->bl[PIXEL(j, (size-1-i), size)] = ll32;
					// First "bevel size" columns are highlight, except for the bottom right corner
					newBorderset->bl[PIXEL(i, j, size)] = hl32;
					
					// Top Right corner:  Entire "bevel size" first rows are highlight
					newBorderset->tr[PIXEL(j, i, size)] = hl32;
					// Last "bevel size" columns are lowlight, except for the top left
					newBorderset->tr[PIXEL((size-1-i), j, size)] = ll32;					

				}
			}
			// Beveled Corner Parts
			for (int i=0; i < bevelSize; i++) {

				for (int j=0; j<bevelSize; j++) {
   		                // Outer Corners
					// Bottom left corner: 
					// First "bevel size" columns are highlight, except for the bottom right corner
					newBorderset->bl[PIXEL(i, (size - 1 - j), size)] = (j >= i) ? hl32 : ll32;
					
					// Top Right corner:
					// Last "bevel size" columns are lowlight, except for the top left
					newBorderset->tr[PIXEL((size-1-i), j, size)] = (j > i) ? ll32 : hl32;
					
					
				// Inner Corners
					// Top left corner:  Bottom right is all dark
					newBorderset->tl[PIXEL((size-1-i), (size - 1 - j), size)] = ll32;
					
                                        // Bottom Right corner:  Top left is all light
					newBorderset->br[PIXEL(i, j, size)] = hl32;
					
					// Top Right corner:
					// Interior bottom left is dark on top, light on bottom
					newBorderset->tr[PIXEL(i, (size-1-j), size)] = (i > j) ? hl32 : ll32;						
					
					// Bottom Left corner:
					// Interior top right is dark on top, light on bottom
					newBorderset->bl[PIXEL((size-1-i), j, size)] = (i > j) ? ll32 : hl32;						

											

				}
			
			
			}

		
		
		
		break;
		
		case BORDER_FLAT:  // Placeholder that uses buffers but for a flat colour
			newBorderset->top = znew(uint32_t);
			newBorderset->left = znew(uint32_t);
			newBorderset->right = znew(uint32_t);		
			newBorderset->bottom = znew(uint32_t);
			*newBorderset->top = id;
			*newBorderset->left = id;
			*newBorderset->right = id;
			*newBorderset->bottom = id;
			
			// Fill with solid
			for (int j=0; j<size;j++) {
				for (int k=0; k<size;k++) {
					newBorderset->tl[PIXEL(j, k, size)] = id;
					newBorderset->tr[PIXEL(size - 1 - j, k, size)] = id;
					newBorderset->bl[PIXEL(size - 1 -j, k, size)] = id;
					newBorderset->br[PIXEL(j, k, size)] = id;
				}
			}
		
		
		
		break;
		
		case BORDER_INSET:  // Sunken Single bevel borders have 1x1 sides
			newBorderset->top = znew(uint32_t);
			newBorderset->left = znew(uint32_t);
			newBorderset->right = znew(uint32_t);		
			newBorderset->bottom = znew(uint32_t);
			*newBorderset->top = ll32;
			*newBorderset->left = ll32;
			*newBorderset->right = hl32;
			*newBorderset->bottom = hl32;
			
			// Fill with solid
			for (int j=0; j<size;j++) {
				for (int k=0; k<size;k++) {
					newBorderset->tl[PIXEL(j, k, size)] = ll32;
					newBorderset->tr[PIXEL(size - 1 - j, k, size)] = (j > k) ? ll32 : hl32;
					newBorderset->bl[PIXEL(size - 1 -j, k, size)] = (j > k) ? ll32 : hl32;
					newBorderset->br[PIXEL(j, k, size)] = hl32;
				}
			}
		
		
		
		break;

	}
	return newBorderset;
}


struct bufferset * generateBufferset(struct wlr_scene_tree * tree, struct borderset *borderset, int bw)
{
	struct bufferset * bufferset = znew(struct bufferset);
	
	bufferset->border_width = bw;
	if (borderset->type == BORDER_DOUBLE) {
		struct lab_data_buffer *ttexture_buffer =
			buffer_create_from_data(borderset->top, 1, bw, 4);
		bufferset->top = wlr_scene_buffer_create(tree, &ttexture_buffer->base);

		struct lab_data_buffer *ltexture_buffer =
			buffer_create_from_data(borderset->left, bw, 1, 4*bw);
		bufferset->left = wlr_scene_buffer_create(tree, &ltexture_buffer->base);

		struct lab_data_buffer *rtexture_buffer =
			buffer_create_from_data(borderset->right, bw, 1, 4*bw);
		bufferset->right = wlr_scene_buffer_create(tree, &rtexture_buffer->base);

		struct lab_data_buffer *btexture_buffer =
			buffer_create_from_data(borderset->bottom, 1, bw, 4);
		bufferset->bottom = wlr_scene_buffer_create(tree, &btexture_buffer->base);
	
	
	} else {
		struct lab_data_buffer *ttexture_buffer =
			buffer_create_from_data(borderset->top, 1, 1, 4);
		bufferset->top = wlr_scene_buffer_create(tree, &ttexture_buffer->base);

		struct lab_data_buffer *ltexture_buffer =
			buffer_create_from_data(borderset->left, 1, 1, 4);
		bufferset->left = wlr_scene_buffer_create(tree, &ltexture_buffer->base);

		struct lab_data_buffer *rtexture_buffer =
			buffer_create_from_data(borderset->right, 1, 1, 4);
		bufferset->right = wlr_scene_buffer_create(tree, &rtexture_buffer->base);

		struct lab_data_buffer *btexture_buffer =
			buffer_create_from_data(borderset->bottom, 1, 1, 4);
		bufferset->bottom = wlr_scene_buffer_create(tree, &btexture_buffer->base);
	}


	struct lab_data_buffer *tltexture_buffer =
		buffer_create_from_data(borderset->tl, bw, bw, 4*bw);
	bufferset->tl = wlr_scene_buffer_create(tree, &tltexture_buffer->base);


	struct lab_data_buffer *trtexture_buffer =
		buffer_create_from_data(borderset->tr, bw, bw, 4*bw);
	bufferset->tr = wlr_scene_buffer_create(tree, &trtexture_buffer->base);


	struct lab_data_buffer *bltexture_buffer =
		buffer_create_from_data(borderset->bl, bw, bw, 4*bw);
	bufferset->bl = wlr_scene_buffer_create(tree, &bltexture_buffer->base);

	struct lab_data_buffer *brtexture_buffer =
		buffer_create_from_data(borderset->br, bw, bw, 4*bw);
	bufferset->br = wlr_scene_buffer_create(tree, &brtexture_buffer->base);

	return bufferset;
}

void renderBufferset(struct bufferset *bufferset, int width, int height, int y)
{

	wlr_scene_buffer_set_dest_size(bufferset->top,
			width - 2 * bufferset->border_width, bufferset->border_width);		
	wlr_scene_node_set_position(&bufferset->top->node,
			bufferset->border_width,y);	

	wlr_scene_buffer_set_dest_size(bufferset->bottom,
			width - 2 * bufferset->border_width, bufferset->border_width);		
	wlr_scene_node_set_position(&bufferset->bottom->node,
			 bufferset->border_width, y+height - bufferset->border_width);


	wlr_scene_buffer_set_dest_size(bufferset->left,
			bufferset->border_width, height - bufferset->border_width * 2);
	wlr_scene_node_set_position(&bufferset->left->node,
			0, bufferset->border_width+y);	

	wlr_scene_buffer_set_dest_size(bufferset->right,
			bufferset->border_width, height - bufferset->border_width * 2);
	wlr_scene_node_set_position(&bufferset->right->node,
			width - bufferset->border_width, y+ bufferset->border_width);	

	wlr_scene_buffer_set_dest_size(bufferset->tl,
		bufferset->border_width, bufferset->border_width);		
	wlr_scene_node_set_position(&bufferset->tl->node,
		0,y);	

	wlr_scene_buffer_set_dest_size(bufferset->tr,
		bufferset->border_width, bufferset->border_width);		
	wlr_scene_node_set_position(&bufferset->tr->node,
		width-bufferset->border_width, y);	


	wlr_scene_buffer_set_dest_size(bufferset->br,
		bufferset->border_width, bufferset->border_width);		
	wlr_scene_node_set_position(&bufferset->br->node,
		width-bufferset->border_width , y+height-bufferset->border_width);	


	wlr_scene_buffer_set_dest_size(bufferset->bl,
		bufferset->border_width, bufferset->border_width);		
	wlr_scene_node_set_position(&bufferset->bl->node,
		0, height-bufferset->border_width+y);	

}

void clearBorderCache(struct borderset * borderset)
{
	if (borderset == NULL)
		return;
	if (borderset->next != NULL) {
		clearBorderCache(borderset->next);
	}
	
	free(borderset->top);
	free(borderset->left);
	free(borderset->right);
	free(borderset->bottom);
	free(borderset->tl);
	free(borderset->tr);
	free(borderset->bl);
	free(borderset->br);
	free(borderset);
}
