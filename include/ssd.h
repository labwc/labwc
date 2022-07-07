/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_SSD_H
#define __LABWC_SSD_H

#include "buffer.h"
#include <wlr/util/box.h>

#define BUTTON_COUNT 4
#define BUTTON_WIDTH 26
#define EXTENDED_AREA 8

#define FOR_EACH(tmp, ...) \
{ \
	__typeof__(tmp) _x[] = { __VA_ARGS__, NULL }; \
	size_t _i = 0; \
	for ((tmp) = _x[_i]; _i < sizeof(_x) / sizeof(_x[0]) - 1; (tmp) = _x[++_i])

#define FOR_EACH_END }

/*
 * Sequence these according to the order they should be processed for
 * press and hover events. Bear in mind that some of their respective
 * interactive areas overlap, so for example buttons need to come before title.
 */
enum ssd_part_type {
	LAB_SSD_NONE = 0,
	LAB_SSD_BUTTON_CLOSE,
	LAB_SSD_BUTTON_MAXIMIZE,
	LAB_SSD_BUTTON_ICONIFY,
	LAB_SSD_BUTTON_WINDOW_MENU,
	LAB_SSD_PART_TITLEBAR,
	LAB_SSD_PART_TITLE,
	LAB_SSD_PART_CORNER_TOP_LEFT,
	LAB_SSD_PART_CORNER_TOP_RIGHT,
	LAB_SSD_PART_CORNER_BOTTOM_RIGHT,
	LAB_SSD_PART_CORNER_BOTTOM_LEFT,
	LAB_SSD_PART_TOP,
	LAB_SSD_PART_RIGHT,
	LAB_SSD_PART_BOTTOM,
	LAB_SSD_PART_LEFT,
	LAB_SSD_CLIENT,
	LAB_SSD_FRAME,
	LAB_SSD_ROOT,
	LAB_SSD_MENU,
	LAB_SSD_OSD,
	LAB_SSD_LAYER_SURFACE,
	LAB_SSD_UNMANAGED,
	LAB_SSD_END_MARKER
};

/* Forward declare arguments */
struct view;
struct wl_list;
struct wlr_box;
struct wlr_scene_tree;
struct scaled_font_buffer;

struct ssd_button {
	struct view *view;
	enum ssd_part_type type;
	struct wlr_scene_node *hover;

	struct wl_listener destroy;
};

struct ssd_sub_tree {
	struct wlr_scene_tree *tree;
	struct wl_list parts; /* ssd_part::link */
};

struct ssd_state_title_width {
	int width;
	bool truncated;
};

struct ssd {
	bool enabled;
	struct wlr_scene_tree *tree;

	/*
	 * Cache for current values.
	 * Used to detect actual changes so we
	 * don't update things we don't have to.
	 */
	struct {
		int x;
		int y;
		int width;
		int height;
		struct ssd_state_title {
			char *text;
			struct ssd_state_title_width active;
			struct ssd_state_title_width inactive;
		} title;
	} state;

	/* An invisble area around the view which allows resizing */
	struct ssd_sub_tree extents;

	/* The top of the view, containing buttons, title, .. */
	struct {
		/* struct wlr_scene_tree *tree;      unused for now */
		struct ssd_sub_tree active;
		struct ssd_sub_tree inactive;
	} titlebar;

	/* Borders allow resizing as well */
	struct {
		/* struct wlr_scene_tree *tree;      unused for now */
		struct ssd_sub_tree active;
		struct ssd_sub_tree inactive;
	} border;
};

struct ssd_part {
	enum ssd_part_type type;

	/* Buffer pointer. May be NULL */
	struct scaled_font_buffer *buffer;

	/* This part represented in scene graph */
	struct wlr_scene_node *node;

	/* Targeted geometry. May be NULL */
	struct wlr_box *geometry;

	struct wl_list link;
};

struct ssd_hover_state {
	struct view *view;
	struct wlr_scene_node *node;
};

/* Public SSD API */
void ssd_create(struct view *view);

void ssd_hide(struct view *view);
void ssd_set_active(struct view *view, bool activated);

void ssd_update_title(struct view *view);
void ssd_update_geometry(struct view *view);
void ssd_reload(struct view *view);
void ssd_destroy(struct view *view);
void ssd_update_button_hover(struct wlr_scene_node *node,
	struct ssd_hover_state *hover_state);

/* Public SSD helpers */
enum ssd_part_type ssd_at(struct view *view, double lx, double ly);
enum ssd_part_type ssd_get_part_type(
	struct view *view, struct wlr_scene_node *node);
uint32_t ssd_resize_edges(enum ssd_part_type type);
bool ssd_is_button(enum ssd_part_type type);
bool ssd_part_contains(enum ssd_part_type whole, enum ssd_part_type candidate);

/* SSD internal helpers to create various SSD elements */
/* TODO: Replace some common args with a struct */
struct ssd_part *add_scene_part(
	struct wl_list *part_list, enum ssd_part_type type);
struct ssd_part *add_scene_rect(
	struct wl_list *list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, int width, int height, int x, int y,
	float color[4]);
struct ssd_part *add_scene_buffer(
	struct wl_list *list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, struct wlr_buffer *buffer, int x, int y);
struct ssd_part *add_scene_button(
	struct wl_list *part_list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, float *bg_color,
	struct wlr_buffer *icon_buffer, int x, struct view *view);
struct ssd_part *add_scene_button_corner(
	struct wl_list *part_list, enum ssd_part_type type,
	enum ssd_part_type corner_type, struct wlr_scene_tree *parent,
	struct wlr_buffer *corner_buffer, struct wlr_buffer *icon_buffer,
	int x, struct view *view);

/* SSD internal helpers */
struct ssd_part *ssd_get_part(
	struct wl_list *part_list, enum ssd_part_type type);
void ssd_destroy_parts(struct wl_list *list);

/* SSD internal */
void ssd_titlebar_create(struct view *view);
void ssd_titlebar_update(struct view *view);
void ssd_titlebar_destroy(struct view *view);

void ssd_border_create(struct view *view);
void ssd_border_update(struct view *view);
void ssd_border_destroy(struct view *view);

void ssd_extents_create(struct view *view);
void ssd_extents_update(struct view *view);
void ssd_extents_destroy(struct view *view);

/* TODO: clean up / update */
struct border ssd_thickness(struct view *view);
struct wlr_box ssd_max_extents(struct view *view);

#endif /* __LABWC_SSD_H */
