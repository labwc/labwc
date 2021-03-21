/*
 * Helpers for view server side decorations
 *
 * Copyright (C) 2020 Johan Malm
 */

#include <assert.h>
#include "config/rcxml.h"
#include "labwc.h"
#include "theme.h"
#include "ssd.h"

#define BORDER_WIDTH (2)

struct border
ssd_thickness(struct view *view)
{
	struct border border = {
		.top = rc.title_height + BORDER_WIDTH,
		.bottom = BORDER_WIDTH,
		.left = BORDER_WIDTH,
		.right = BORDER_WIDTH,
	};
	return border;
}

struct wlr_box
ssd_max_extents(struct view *view)
{
	struct border border = ssd_thickness(view);
	struct wlr_box box = {
		.x = view->x - border.left,
		.y = view->y - border.top,
		.width = view->w + border.left + border.right,
		.height = view->h + border.top + border.bottom,
	};
	return box;
}

struct wlr_box
ssd_box(struct view *view, enum ssd_part_type type)
{
	struct wlr_box box = { 0 };
	assert(view);
	switch (type) {
	case LAB_SSD_BUTTON_CLOSE:
		box.width = rc.title_height;
		box.height = rc.title_height;
		box.x = view->x + view->w - rc.title_height;
		box.y = view->y - rc.title_height;
		break;
	case LAB_SSD_BUTTON_MAXIMIZE:
		box.width = rc.title_height;
		box.height = rc.title_height;
		box.x = view->x + view->w - rc.title_height * 2;
		box.y = view->y - rc.title_height;
		break;
	case LAB_SSD_BUTTON_ICONIFY:
		box.width = rc.title_height;
		box.height = rc.title_height;
		box.x = view->x + view->w - rc.title_height * 3;
		box.y = view->y - rc.title_height;
		break;
	case LAB_SSD_PART_TITLE:
		box.x = view->x;
		box.y = view->y - rc.title_height;
		box.width = view->w;
		box.height = rc.title_height;
		break;
	case LAB_SSD_PART_TOP:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y - rc.title_height - BORDER_WIDTH;
		box.width = view->w + 2 * BORDER_WIDTH;
		box.height = BORDER_WIDTH;
		break;
	case LAB_SSD_PART_RIGHT:
		box.x = view->x + view->w;
		box.y = view->y - rc.title_height;
		box.width = BORDER_WIDTH;
		box.height = view->h + rc.title_height;
		break;
	case LAB_SSD_PART_BOTTOM:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y + view->h;
		box.width = view->w + 2 * BORDER_WIDTH;
		box.height = +BORDER_WIDTH;
		break;
	case LAB_SSD_PART_LEFT:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y - rc.title_height;
		box.width = BORDER_WIDTH;
		box.height = view->h + rc.title_height;
		break;
	default:
		break;
	}
	return box;
}

enum ssd_part_type
ssd_at(struct view *view, double lx, double ly)
{
	enum ssd_part_type type;
	for (type = 0; type < LAB_SSD_END_MARKER; ++type) {
		struct wlr_box box = ssd_box(view, type);
		if (wlr_box_contains_point(&box, lx, ly)) {
			return type;
		}
	}
	return LAB_SSD_NONE;
}

static struct ssd_part *
add_part(struct view *view, enum ssd_part_type type)
{
	struct ssd_part *part = calloc(1, sizeof(struct ssd_part));
	part->type = type;
	wl_list_insert(&view->ssd.parts, &part->link);
	return part;
}

void
ssd_create(struct view *view)
{
	struct theme *theme = view->server->theme;
	struct ssd_part *part;

	view->ssd.box.x = view->x;
	view->ssd.box.y = view->y;
	view->ssd.box.width = view->w;
	view->ssd.box.height = view->h;

	/* border */
	float *color = theme->window_active_handle_bg_color;
	enum ssd_part_type border[4] = {
		LAB_SSD_PART_TOP,
		LAB_SSD_PART_RIGHT,
		LAB_SSD_PART_BOTTOM,
		LAB_SSD_PART_LEFT,
	};
	for (int i = 0; i < 4; i++) {
		part = add_part(view, border[i]);
		part->box = ssd_box(view, border[i]);
		part->color = color;
	}
}

void
ssd_destroy(struct view *view)
{
	struct ssd_part *part, *next;
	wl_list_for_each_safe(part, next, &view->ssd.parts, link) {
		wl_list_remove(&part->link);
		free(part);
	}
}

static bool
geometry_changed(struct view *view)
{
	return view->x != view->ssd.box.x || view->y != view->ssd.box.y ||
		view->w != view->ssd.box.width ||
		view->h != view->ssd.box.height;
}

void
ssd_update_geometry(struct view *view)
{
	if (!geometry_changed(view)) {
		return;
	}
	struct ssd_part *part;
	wl_list_for_each(part, &view->ssd.parts, link) {
		part->box = ssd_box(view, part->type);
	}
	view->ssd.box.x = view->x;
	view->ssd.box.y = view->y;
	view->ssd.box.width = view->w;
	view->ssd.box.height = view->h;
	damage_all_outputs(view->server);
}
