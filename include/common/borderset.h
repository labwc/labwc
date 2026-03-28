#include <stdint.h>
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BORDERSET_H
#define LABWC_BORDERSET_H

struct borderset {
	uint32_t id;		// Base colour, but could be used as a tracking hash for images or whatever in the future
	int size;		// width (since I suspect a 2px border scaled up to 20px might look weird)
	int type;			// Single or double bevel
	uint32_t * top;
	uint32_t * left;
	uint32_t * right;
	uint32_t * bottom;
	uint32_t * tl;
	uint32_t * tr;
	uint32_t * bl;
	uint32_t * br;
	struct borderset * next;
};


extern struct borderset * borderCache;

struct borderset * getBorders(uint32_t id, int size, int type, int bevelSize);

struct borderset * createBuffer(uint32_t id, int size, int type, int bevelSize);

void clearBorderCache(struct borderset *borderset);

#endif /* LABWC_LAB_SCENE_RECT_H */
