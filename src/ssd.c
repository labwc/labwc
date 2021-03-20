/*
 * Helpers for handling window decorations
 *
 * Copyright Johan Malm 2020
 */

#include <assert.h>
#include "config/rcxml.h"
#include "labwc.h"

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
ssd_box(struct view *view, enum ssd_part ssd_part)
{
	struct wlr_box box = { 0 };
	assert(view);
	switch (ssd_part) {
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

enum ssd_part
ssd_at(struct view *view, double lx, double ly)
{
	enum ssd_part ssd_part;
	for (ssd_part = 0; ssd_part < LAB_SSD_END_MARKER; ++ssd_part) {
		struct wlr_box box = ssd_box(view, ssd_part);
		if (wlr_box_contains_point(&box, lx, ly)) {
			return ssd_part;
		}
	}
	return LAB_SSD_NONE;
}
