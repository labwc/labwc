#include <stdint.h>
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BORDERSET_H
#define LABWC_BORDERSET_H

enum border_type {
	BORDER_FLAT, BORDER_SINGLE, BORDER_DOUBLE, BORDER_INSET
};

struct borderset {
	uint32_t id;		// Base colour, but could be used as a tracking hash for images or whatever in the future
	int size;		// width (since I suspect a 2px border scaled up to 20px might look weird)
	enum border_type type;			// Single or double bevel
	struct lab_data_buffer *top;
	struct lab_data_buffer *left;
	struct lab_data_buffer *right;
	struct lab_data_buffer *bottom;
	struct lab_data_buffer *tl;
	struct lab_data_buffer *tr;
	struct lab_data_buffer *bl;
	struct lab_data_buffer *br;
	struct borderset * next;
};

struct bufferset {
	enum border_type type;
	int border_width;
	struct wlr_scene_buffer * top;
	struct wlr_scene_buffer * left;
	struct wlr_scene_buffer * right;
	struct wlr_scene_buffer * bottom;
	struct wlr_scene_buffer * tl;
	struct wlr_scene_buffer * tr;
	struct wlr_scene_buffer * bl;
	struct wlr_scene_buffer * br;

};

extern struct borderset * borderCache;

struct borderset * getBorders(uint32_t id, int size, enum border_type, int bevelSize);

struct borderset * createBuffer(uint32_t id, int size, enum border_type, int bevelSize);

struct bufferset * generateBufferset(struct wlr_scene_tree * tree, struct borderset *borderset, int bw);

void renderBufferset(struct bufferset *, int width, int height, int y);

void clearBorderCache(struct borderset *borderset);

#endif /* LABWC_LAB_SCENE_RECT_H */
