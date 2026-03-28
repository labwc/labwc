#include "common/borderset.h"
#include "common/mem.h"
#include "common/macros.h"


struct borderset * getBorders(uint32_t id, int size, int type) {
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
		borderCache = createBuffer(id, size, type);
		return borderCache;
	} else {
		last->next = createBuffer(id, size, type);
		return last->next;
	}
	return NULL;
}

struct borderset * createBuffer(uint32_t id, int size, int type) {
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
	
	
	newBorderset->tl = znew_n(uint32_t, size*size);
	newBorderset->tr = znew_n(uint32_t, size*size);
	newBorderset->bl = znew_n(uint32_t, size*size);
	newBorderset->br = znew_n(uint32_t, size*size);
	newBorderset->top = znew(uint32_t);
	newBorderset->left = znew(uint32_t);
	newBorderset->right = znew(uint32_t);		
	newBorderset->bottom = znew(uint32_t);			
	switch(type) {
		case 1:
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
