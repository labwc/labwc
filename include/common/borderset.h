/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <wlr/types/wlr_scene.h>

#ifndef LABWC_BORDERSET_H
#define LABWC_BORDERSET_H

enum border_type {
	BORDER_NONE, BORDER_SINGLE, BORDER_DOUBLE, BORDER_INSET, BORDER_DOUBLE_INSET, BORDER_FLAT
};

struct borderset {
	// Base colour, but could be used as a tracking hash for images or whatever in the future
	uint32_t id;
	// width (since I suspect a 2px border scaled up to 20px might look weird)
	int size;
	// Single or double bevel, etc.
	enum border_type type;
	// So we can disambiguate multiple possible designs cached together
	int bevelSize;
	struct lab_data_buffer *top;
	struct lab_data_buffer *left;
	struct lab_data_buffer *right;
	struct lab_data_buffer *bottom;
	struct lab_data_buffer *tl;
	struct lab_data_buffer *tr;
	struct lab_data_buffer *bl;
	struct lab_data_buffer *br;
	struct borderset *next;
};

struct bufferset {
	enum border_type type;
	int border_width;
	struct wlr_scene_buffer *top;
	struct wlr_scene_buffer *left;
	struct wlr_scene_buffer *right;
	struct wlr_scene_buffer *bottom;
	struct wlr_scene_buffer *tl;
	struct wlr_scene_buffer *tr;
	struct wlr_scene_buffer *bl;
	struct wlr_scene_buffer *br;

};

extern struct borderset *border_cache;

struct borderset *get_borders(uint32_t id, int size, enum border_type, int bevelSize);

struct borderset *create_buffer(uint32_t id, int size, enum border_type, int bevelSize);

struct bufferset *generate_bufferset(struct wlr_scene_tree *tree,
	struct borderset *borderset, int bw);

void renderBufferset(struct bufferset *bufferset, int width, int height, int y);

void renderBuffersetXY(struct bufferset *bufferset, int width, int height, int x, int y);

void clearborder_cache(struct borderset *borderset);

#endif /* LABWC_BORDERSET_H */
