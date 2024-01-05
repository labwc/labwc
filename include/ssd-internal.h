/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SSD_INTERNAL_H
#define LABWC_SSD_INTERNAL_H

#include <wlr/util/box.h>
#include "common/macros.h"
#include "ssd.h"
#include "view.h"

#define FOR_EACH(tmp, ...) \
{ \
	__typeof__(tmp) _x[] = { __VA_ARGS__, NULL }; \
	size_t _i = 0; \
	for ((tmp) = _x[_i]; _i < ARRAY_SIZE(_x) - 1; (tmp) = _x[++_i])

#define FOR_EACH_END }

struct ssd_button {
	struct view *view;
	enum ssd_part_type type;
	struct wlr_scene_node *normal;
	struct wlr_scene_node *hover;
	struct wlr_scene_node *toggled;
	struct wlr_scene_node *toggled_hover;
	struct wlr_scene_node *background;
	struct wlr_scene_tree *icon_tree;
	struct wlr_scene_tree *hover_tree;

	struct wl_listener destroy;
};

struct ssd_sub_tree {
	struct wlr_scene_tree *tree;
	struct wl_list parts; /* ssd_part.link */
};

struct ssd_state_title_width {
	int width;
	bool truncated;
};

struct ssd {
	struct view *view;
	struct wlr_scene_tree *tree;

	/*
	 * Cache for current values.
	 * Used to detect actual changes so we
	 * don't update things we don't have to.
	 */
	struct {
		bool was_maximized;   /* To un-round corner buttons and toggle icon on maximize */
		struct wlr_box geometry;
		struct ssd_state_title {
			char *text;
			struct ssd_state_title_width active;
			struct ssd_state_title_width inactive;
		} title;
	} state;

	/* An invisible area around the view which allows resizing */
	struct ssd_sub_tree extents;

	/* The top of the view, containing buttons, title, .. */
	struct {
		int height;
		struct wlr_scene_tree *tree;
		struct ssd_sub_tree active;
		struct ssd_sub_tree inactive;
	} titlebar;

	/* Borders allow resizing as well */
	struct {
		struct wlr_scene_tree *tree;
		struct ssd_sub_tree active;
		struct ssd_sub_tree inactive;
	} border;

	/*
	 * Space between the extremities of the view's wlr_surface
	 * and the max extents of the server-side decorations.
	 * For xdg-shell views with CSD, this margin is zero.
	 */
	struct border margin;
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
	struct ssd_button *button;
};

struct wlr_buffer;
struct wlr_scene_tree;

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
	struct wlr_buffer *icon_buffer, struct wlr_buffer *hover_buffer,
	int x, struct view *view);
void
add_toggled_icon(struct wl_list *part_list, enum ssd_part_type type,
		struct wlr_buffer *icon_buffer, struct wlr_buffer *hover_buffer);
struct ssd_part *add_scene_button_corner(
	struct wl_list *part_list, enum ssd_part_type type,
	enum ssd_part_type corner_type, struct wlr_scene_tree *parent,
	float *bg_color, struct wlr_buffer *corner_buffer, struct wlr_buffer *icon_buffer,
	struct wlr_buffer *hover_buffer, int x, struct view *view);

/* SSD internal helpers */
struct ssd_part *ssd_get_part(
	struct wl_list *part_list, enum ssd_part_type type);
void ssd_destroy_parts(struct wl_list *list);

/* SSD internal */
void ssd_titlebar_create(struct ssd *ssd);
void ssd_titlebar_update(struct ssd *ssd);
void ssd_titlebar_destroy(struct ssd *ssd);

void ssd_border_create(struct ssd *ssd);
void ssd_border_update(struct ssd *ssd);
void ssd_border_destroy(struct ssd *ssd);

void ssd_extents_create(struct ssd *ssd);
void ssd_extents_update(struct ssd *ssd);
void ssd_extents_destroy(struct ssd *ssd);

#endif /* LABWC_SSD_INTERNAL_H */
