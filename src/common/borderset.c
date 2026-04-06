// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include "common/borderset.h"
#include "common/mem.h"
#include "common/macros.h"
#include "buffer.h"

struct borderset *getBorders(uint32_t id, int size, enum border_type type, int bevelSize)
{
	struct borderset *current = borderCache;
	struct borderset *last;

	// Preventing building nonsense borders:
	// If you try for a double bevel but it's so deep they would overlap,
	// convert to a single bevel
	if (type == BORDER_DOUBLE && (bevelSize > size/2)) {
		type = BORDER_SINGLE;
	}

	if (type == BORDER_DOUBLE_INSET && (bevelSize > size/2)) {
		type = BORDER_INSET;
	}

	// Anything with a size of 0 is converted to a 1-pixel flat border so as to
	// prevent empty allocations

	if (size < 1) {
		type = BORDER_FLAT;
		size = 1;
	}

	while (current) {
		if (current->size == size && current->id == id &&
			current->type == type && current->bevelSize == bevelSize) {
			return current;
		}
		last = current;
		current = current->next;
	}
	// Fall through, we need to create a buffer.

	if (!borderCache) {
		borderCache = createBuffer(id, size, type, bevelSize);
		return borderCache;
	} else {
		last->next = createBuffer(id, size, type, bevelSize);
		return last->next;
	}
	return NULL;
}

struct borderset *createBuffer(uint32_t id, int size, enum border_type type, int bevelSize)
{
	struct borderset *new_borderset = znew(*new_borderset);

	new_borderset->next = NULL;
	new_borderset->id = id;
	new_borderset->size = size;
	new_borderset->type = type;
	new_borderset->bevelSize = bevelSize;

	// Use ID as a AARRGGBB colour
	uint8_t a = id >> 24 & 255;
	uint8_t r = id >> 16 & 255;
	uint8_t g = id >> 8 & 255;
	uint8_t b = id & 255;

	uint32_t r1 = r * 5 / 4;
	if (r1 > a) {
		r1 = a;
	}
	uint32_t g1 = g * 5 / 4;
	if (g1 > a) {
		g1 = a;
	}
	uint32_t b1 = b * 5 / 4;
	if (b1 > a) {
		b1 = a;
	}

	/* darker outline */
	uint32_t r0 = r / 2;
	uint32_t g0 = g / 2;
	uint32_t b0 = b / 2;

	uint32_t hl32 = ((uint32_t)a << 24) | ((uint32_t)r1 << 16)
				| ((uint32_t)g1 << 8) | (uint32_t)b1;
	uint32_t ll32 = ((uint32_t)a << 24) | ((uint32_t)r0 << 16)
				| ((uint32_t)g0 << 8) | (uint32_t)b0;
	uint32_t temp;

	// All borders have NxN corners
	uint32_t *tl = znew_n(uint32_t, size*size);
	uint32_t *tr = znew_n(uint32_t, size*size);
	uint32_t *bl = znew_n(uint32_t, size*size);
	uint32_t *br = znew_n(uint32_t, size*size);
	uint32_t *top = NULL;
	uint32_t *left = NULL;
	uint32_t *right = NULL;
	uint32_t *bottom = NULL;
	size_t side_size = 0;

	switch (type) {
	case BORDER_INSET:
		temp = ll32;
		ll32 = hl32;
		hl32 = temp;
		// Fall throgh intentional
	case BORDER_SINGLE:  // Single bevel borders have 1x1 sides
		top = znew(uint32_t);
		left = znew(uint32_t);
		right = znew(uint32_t);
		bottom = znew(uint32_t);
		side_size = 1;
		*top = hl32;
		*left = hl32;
		*right = ll32;
		*bottom = ll32;

		// Fill with solid
		for (int j = 0; j < size; j++) {
			for (int k = 0; k < size; k++) {
				tl[PIXEL(j, k, size)] = hl32;
				tr[PIXEL(size - 1 - j, k, size)] = (j > k) ? hl32 : ll32;
				bl[PIXEL(size - 1 -j, k, size)] = (j > k) ? hl32 : ll32;
				br[PIXEL(j, k, size)] = ll32;
			}
		}

	break;

	case BORDER_DOUBLE_INSET:
		temp = ll32;
		ll32 = hl32;
		hl32 = temp;
		// Fall throgh intentional
	case BORDER_DOUBLE:
		top = znew_n(uint32_t, size);
		left = znew_n(uint32_t, size);
		right = znew_n(uint32_t, size);
		bottom = znew_n(uint32_t, size);
		side_size = size;

		for (int i = 0; i < size; i++) {
			if (i < bevelSize) {
				left[i] = hl32;
				top[i] = hl32;
				right[i] = hl32;
				bottom[i] = hl32;

			} else if (i > (size-bevelSize-1)) {
				left[i] = ll32;
				top[i] = ll32;
				right[i] = ll32;
				bottom[i] = ll32;

			} else {
				left[i] = id;
				top[i] = id;
				right[i] = id;
				bottom[i] = id;
			}
		}

		// Blank corners...
		for (int i = 0; i < size; i++) {
			for (int j = 0; j < size; j++) {
				tl[PIXEL(i, j, size)] = id;
				tr[PIXEL(i, j, size)] = id;
				bl[PIXEL(i, j, size)] = id;
				br[PIXEL(i, j, size)] = id;
			}
		}

		// Main Corners
		for (int i = 0; i < bevelSize; i++) {
			// Solid bar parts
			for (int j = 0; j < size; j++) {
				// Top left corner:  Entire "bevel size" top rows are highlighted
				tl[PIXEL(j, i, size)] = hl32;
				// First "bevel size" top columns are highlighted
				tl[PIXEL(i, j, size)] = hl32;

				// Bottom Right corner:  Entire "bevel size" last rows are lowlight
				br[PIXEL(j, (size-1-i), size)] = ll32;
				// Last "bevel size" columns are lowlight
				br[PIXEL((size-1-i), j, size)] = ll32;

				// Bottom left corner:  Entire "bevel size" last rows are lowlight
				bl[PIXEL(j, (size-1-i), size)] = ll32;
				// First "bevel size" columns are highlight, except for
				// the bottom right corner
				bl[PIXEL(i, j, size)] = hl32;

				// Top Right corner:  Entire "bevel size" first rows are highlight
				tr[PIXEL(j, i, size)] = hl32;
				// Last "bevel size" columns are lowlight, except for the top left
				tr[PIXEL((size-1-i), j, size)] = ll32;
			}
		}
		// Beveled Corner Parts
		for (int i = 0; i < bevelSize; i++) {
			for (int j = 0; j < bevelSize; j++) {
					// Outer Corners
				// Bottom left corner:
				// First "bevel size" columns are highlight, except
				// for the bottom right corner
				bl[PIXEL(i, (size - 1 - j), size)] = (j >= i) ? hl32 : ll32;

				// Top Right corner:
				// Last "bevel size" columns are lowlight, except for the top left
				tr[PIXEL((size-1-i), j, size)] = (j > i) ? ll32 : hl32;

			// Inner Corners
				// Top left corner:  Bottom right is all dark
				tl[PIXEL((size-1-i), (size - 1 - j), size)] = ll32;

				// Bottom Right corner:  Top left is all light
				br[PIXEL(i, j, size)] = hl32;

				// Top Right corner:
				// Interior bottom left is dark on top, light on bottom
				tr[PIXEL(i, (size-1-j), size)] = (i > j) ? hl32 : ll32;

				// Bottom Left corner:
				// Interior top right is dark on top, light on bottom
				bl[PIXEL((size-1-i), j, size)] = (i > j) ? ll32 : hl32;
			}
		}

		break;

	case BORDER_FLAT:  // Placeholder that uses buffers but for a flat colour
	case BORDER_NONE:  // Provided as a fallback but should not be actually requested/rendered
		top = znew(uint32_t);
		left = znew(uint32_t);
		right = znew(uint32_t);
		bottom = znew(uint32_t);
		side_size = 1;
		*top = id;
		*left = id;
		*right = id;
		*bottom = id;

		// Fill with solid
		for (int j = 0; j < size; j++) {
			for (int k = 0; k < size; k++) {
				tl[PIXEL(j, k, size)] = id;
				tr[PIXEL(size - 1 - j, k, size)] = id;
				bl[PIXEL(size - 1 -j, k, size)] = id;
				br[PIXEL(j, k, size)] = id;
			}
		}

		break;
	}
	assert(side_size > 0);

	new_borderset->top = buffer_create_from_data(top, 1, side_size, 4);
	new_borderset->left = buffer_create_from_data(left, side_size, 1, side_size * 4);
	new_borderset->right = buffer_create_from_data(right, side_size, 1, side_size * 4);
	new_borderset->bottom = buffer_create_from_data(bottom, 1, side_size, 4);

	new_borderset->tl = buffer_create_from_data(tl, size, size, size * 4);
	new_borderset->tr = buffer_create_from_data(tr, size, size, size * 4);
	new_borderset->bl = buffer_create_from_data(bl, size, size, size * 4);
	new_borderset->br = buffer_create_from_data(br, size, size, size * 4);

	return new_borderset;
}

struct bufferset *generateBufferset(struct wlr_scene_tree *tree,
	struct borderset *borderset, int bw)
{
	struct bufferset *bufferset = znew(struct bufferset);

	bufferset->top = wlr_scene_buffer_create(tree, &borderset->top->base);
	bufferset->left = wlr_scene_buffer_create(tree, &borderset->left->base);
	bufferset->right = wlr_scene_buffer_create(tree, &borderset->right->base);
	bufferset->bottom = wlr_scene_buffer_create(tree, &borderset->bottom->base);
	bufferset->tl = wlr_scene_buffer_create(tree, &borderset->tl->base);
	bufferset->tr = wlr_scene_buffer_create(tree, &borderset->tr->base);
	bufferset->bl = wlr_scene_buffer_create(tree, &borderset->bl->base);
	bufferset->br = wlr_scene_buffer_create(tree, &borderset->br->base);

	bufferset->border_width = bw;

	wlr_scene_buffer_set_filter_mode(bufferset->top, WLR_SCALE_FILTER_NEAREST);
	wlr_scene_buffer_set_filter_mode(bufferset->left, WLR_SCALE_FILTER_NEAREST);
	wlr_scene_buffer_set_filter_mode(bufferset->right, WLR_SCALE_FILTER_NEAREST);
	wlr_scene_buffer_set_filter_mode(bufferset->bottom, WLR_SCALE_FILTER_NEAREST);
	wlr_scene_buffer_set_filter_mode(bufferset->tl, WLR_SCALE_FILTER_NEAREST);
	wlr_scene_buffer_set_filter_mode(bufferset->tr, WLR_SCALE_FILTER_NEAREST);
	wlr_scene_buffer_set_filter_mode(bufferset->bl, WLR_SCALE_FILTER_NEAREST);
	wlr_scene_buffer_set_filter_mode(bufferset->br, WLR_SCALE_FILTER_NEAREST);

	return bufferset;
}

void renderBufferset(struct bufferset *bufferset, int width, int height, int y)
{
	renderBuffersetXY(bufferset, width, height, 0, y);
}

void renderBuffersetXY(struct bufferset *bufferset, int width, int height, int x, int y)
{
	wlr_scene_buffer_set_dest_size(bufferset->top,
			width - 2 * bufferset->border_width, bufferset->border_width);
	wlr_scene_node_set_position(&bufferset->top->node,
			x+bufferset->border_width, y);

	wlr_scene_buffer_set_dest_size(bufferset->bottom,
			width - 2 * bufferset->border_width, bufferset->border_width);
	wlr_scene_node_set_position(&bufferset->bottom->node,
			x+bufferset->border_width, y+height - bufferset->border_width);

	wlr_scene_buffer_set_dest_size(bufferset->left,
			bufferset->border_width, height - bufferset->border_width * 2);
	wlr_scene_node_set_position(&bufferset->left->node,
			x, bufferset->border_width+y);

	wlr_scene_buffer_set_dest_size(bufferset->right,
			bufferset->border_width, height - bufferset->border_width * 2);
	wlr_scene_node_set_position(&bufferset->right->node,
			x+width - bufferset->border_width, y+bufferset->border_width);

	wlr_scene_buffer_set_dest_size(bufferset->tl,
		bufferset->border_width, bufferset->border_width);
	wlr_scene_node_set_position(&bufferset->tl->node,
		x, y);

	wlr_scene_buffer_set_dest_size(bufferset->tr,
		bufferset->border_width, bufferset->border_width);
	wlr_scene_node_set_position(&bufferset->tr->node,
		x+width-bufferset->border_width, y);

	wlr_scene_buffer_set_dest_size(bufferset->br,
		bufferset->border_width, bufferset->border_width);
	wlr_scene_node_set_position(&bufferset->br->node,
		x+width-bufferset->border_width, y+height-bufferset->border_width);

	wlr_scene_buffer_set_dest_size(bufferset->bl,
		bufferset->border_width, bufferset->border_width);
	wlr_scene_node_set_position(&bufferset->bl->node,
		x, height-bufferset->border_width+y);
}

void clearBorderCache(struct borderset *borderset)
{
	if (!borderset) {
		return;
	}
	if (borderset->next) {
		clearBorderCache(borderset->next);
	}
	wlr_buffer_drop(&borderset->top->base);
	wlr_buffer_drop(&borderset->left->base);
	wlr_buffer_drop(&borderset->right->base);
	wlr_buffer_drop(&borderset->bottom->base);
	wlr_buffer_drop(&borderset->tl->base);
	wlr_buffer_drop(&borderset->tr->base);
	wlr_buffer_drop(&borderset->bl->base);
	wlr_buffer_drop(&borderset->br->base);
	free(borderset);
}
