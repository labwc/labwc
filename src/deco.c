/*
 * Helpers for handling window decorations
 *
 * Copyright Johan Malm 2020
 */

#include <assert.h>
#include "config/rcxml.h"
#include "labwc.h"
#include "theme/theme.h"

#define BORDER_WIDTH (2)

struct border
deco_thickness(struct view *view)
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
deco_max_extents(struct view *view)
{
	struct border border = deco_thickness(view);
	struct wlr_box box = {
		.x = view->x - border.left,
		.y = view->y - border.top,
		.width = view->w + border.left + border.right,
		.height = view->h + border.top + border.bottom,
	};
	return box;
}

struct wlr_box
deco_box(struct view *view, enum deco_part deco_part)
{
	struct wlr_box box = { 0 };
	assert(view);
	switch (deco_part) {
	case LAB_DECO_BUTTON_CLOSE:
		box.width = rc.title_height;
		box.height = rc.title_height;
		box.x = view->x + view->w - rc.title_height;
		box.y = view->y - rc.title_height;
		break;
	case LAB_DECO_BUTTON_MAXIMIZE:
		box.width = rc.title_height;
		box.height = rc.title_height;
		box.x = view->x + view->w - rc.title_height * 2;
		box.y = view->y - rc.title_height;
		break;
	case LAB_DECO_BUTTON_ICONIFY:
		box.width = rc.title_height;
		box.height = rc.title_height;
		box.x = view->x + view->w - rc.title_height * 3;
		box.y = view->y - rc.title_height;
		break;
	case LAB_DECO_PART_TITLE:
		box.x = view->x;
		box.y = view->y - rc.title_height;
		box.width = view->w;
		box.height = rc.title_height;
		break;
	case LAB_DECO_PART_TOP:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y - rc.title_height - BORDER_WIDTH;
		box.width = view->w + 2 * BORDER_WIDTH;
		box.height = BORDER_WIDTH;
		break;
	case LAB_DECO_PART_RIGHT:
		box.x = view->x + view->w;
		box.y = view->y - rc.title_height;
		box.width = BORDER_WIDTH;
		box.height = view->h + rc.title_height;
		break;
	case LAB_DECO_PART_BOTTOM:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y + view->h;
		box.width = view->w + 2 * BORDER_WIDTH;
		box.height = +BORDER_WIDTH;
		break;
	case LAB_DECO_PART_LEFT:
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

enum deco_part
deco_at(struct view *view, double lx, double ly)
{
	enum deco_part deco_part;
	for (deco_part = 0; deco_part < LAB_DECO_END_MARKER; ++deco_part) {
		struct wlr_box box = deco_box(view, deco_part);
		if (wlr_box_contains_point(&box, lx, ly)) {
			return deco_part;
		}
	}
	return LAB_DECO_NONE;
}
