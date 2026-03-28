#include "common/borderset.h"
#include "common/mem.h"
#include "common/macros.h"

struct borderset * getBorders(uint32_t id, int size, int type, int bevelSize) {
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

struct borderset * createBuffer(uint32_t id, int size, int type, int bevelSize) {
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
		case 1:  // Single bevel borders have 1x1 sides
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
					newBorderset->tl[PIXELSIZED(j, k, size)] = hl32;
					newBorderset->tr[PIXELSIZED(size - 1 - j, k, size)] = (j > k) ? hl32 : ll32;
					newBorderset->bl[PIXELSIZED(size - 1 -j, k, size)] = (j > k) ? hl32 : ll32;
					newBorderset->br[PIXELSIZED(j, k, size)] = ll32;
				}
			}
		
		
		
		break;


		case 2:
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
					newBorderset->tl[PIXELSIZED(i, j, size)] = id;
					newBorderset->tr[PIXELSIZED(i, j, size)] = id;
					newBorderset->bl[PIXELSIZED(i, j, size)] = id;
					newBorderset->br[PIXELSIZED(i, j, size)] = id;
				}
			}

		
			// Main Corners
			for (int i=0; i < bevelSize; i++) {
			
				// Solid bar parts
				for (int j=0; j<size; j++) {
					// Top left corner:  Entire "bevel size" top rows are highlighted
					newBorderset->tl[PIXELSIZED(j, i, size)] = hl32;
					// First "bevel size" top columns are highlighted
					newBorderset->tl[PIXELSIZED(i, j, size)] = hl32;
									
					// Bottom Right corner:  Entire "bevel size" last rows are lowlight
					newBorderset->br[PIXELSIZED(j, (size-1-i), size)] = ll32;
					// Last "bevel size" columns are lowlight
					newBorderset->br[PIXELSIZED((size-1-i), j, size)] = ll32;
				
				
					// Bottom left corner:  Entire "bevel size" last rows are lowlight
					newBorderset->bl[PIXELSIZED(j, (size-1-i), size)] = ll32;
					// First "bevel size" columns are highlight, except for the bottom right corner
					newBorderset->bl[PIXELSIZED(i, j, size)] = hl32;
					
					// Top Right corner:  Entire "bevel size" first rows are highlight
					newBorderset->tr[PIXELSIZED(j, i, size)] = hl32;
					// Last "bevel size" columns are lowlight, except for the top left
					newBorderset->tr[PIXELSIZED((size-1-i), j, size)] = ll32;					

				}
			}
			// Beveled Corner Parts
			for (int i=0; i < bevelSize; i++) {

				for (int j=0; j<bevelSize; j++) {
   		                // Outer Corners
					// Bottom left corner: 
					// First "bevel size" columns are highlight, except for the bottom right corner
					newBorderset->bl[PIXELSIZED(i, (size - 1 - j), size)] = (j >= i) ? hl32 : ll32;
					
					// Top Right corner:
					// Last "bevel size" columns are lowlight, except for the top left
					newBorderset->tr[PIXELSIZED((size-1-i), j, size)] = (j > i) ? ll32 : hl32;
					
					
				// Inner Corners
					// Top left corner:  Bottom right is all dark
					newBorderset->tl[PIXELSIZED((size-1-i), (size - 1 - j), size)] = ll32;
					
                                        // Bottom Right corner:  Top left is all light
					newBorderset->br[PIXELSIZED(i, j, size)] = hl32;
					
					// Top Right corner:
					// Interior bottom left is dark on top, light on bottom
					newBorderset->tr[PIXELSIZED(i, (size-1-j), size)] = (i > j) ? hl32 : ll32;						
					
					// Bottom Left corner:
					// Interior top right is dark on top, light on bottom
					newBorderset->bl[PIXELSIZED((size-1-i), j, size)] = (i > j) ? ll32 : hl32;						

											

				}
			
			
			}

		
		
		
		break;

	}
	return newBorderset;
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
