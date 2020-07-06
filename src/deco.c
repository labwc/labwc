/*
 * Helpers for handling window decorations
 *
 * Copyright Johan Malm 2020
 */

#include "labwc.h"
#include "theme.h"

/* Based on expected font height of Sans 8 */
#define TITLE_HEIGHT (14)
#define BORDER_WIDTH (1)

struct wlr_box deco_max_extents(struct view *view)
{
	struct wlr_box box = {
		.x = view->x - BORDER_WIDTH,
		.y = view->y - TITLE_HEIGHT - BORDER_WIDTH,
		.width = view->surface->current.width + 2 * BORDER_WIDTH,
		.height = view->surface->current.height + TITLE_HEIGHT +
			  2 * BORDER_WIDTH,
	};
	return box;
}

struct wlr_box deco_box(struct view *view, enum deco_part deco_part)
{
	int margin;

	struct wlr_box box = { .x = 0, .y = 0, .width = 0, .height = 0 };
	if (!view || !view->surface)
		return box;
	switch (deco_part) {
	case LAB_DECO_BUTTON_CLOSE:
		wlr_texture_get_size(theme.xbm_close, &box.width, &box.height);
		margin = (TITLE_HEIGHT - box.height) / 2;
		box.x = view->x + view->surface->current.width + margin -
			TITLE_HEIGHT;
		box.y = view->y - TITLE_HEIGHT + margin;
		break;
	case LAB_DECO_BUTTON_MAXIMIZE:
		wlr_texture_get_size(theme.xbm_maximize, &box.width,
				     &box.height);
		margin = (TITLE_HEIGHT - box.height) / 2;
		box.x = view->x + view->surface->current.width + margin -
			TITLE_HEIGHT * 2;
		box.y = view->y - TITLE_HEIGHT + margin;
		break;
	case LAB_DECO_BUTTON_ICONIFY:
		wlr_texture_get_size(theme.xbm_iconify, &box.width,
				     &box.height);
		margin = (TITLE_HEIGHT - box.height) / 2;
		box.x = view->x + view->surface->current.width + margin -
			TITLE_HEIGHT * 3;
		box.y = view->y - TITLE_HEIGHT + margin;
		break;
	case LAB_DECO_PART_TITLE:
		box.x = view->x;
		box.y = view->y - TITLE_HEIGHT;
		box.width = view->surface->current.width;
		box.height = TITLE_HEIGHT;
		break;
	case LAB_DECO_PART_TOP:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y - TITLE_HEIGHT - BORDER_WIDTH;
		box.width = view->surface->current.width + 2 * BORDER_WIDTH;
		box.height = BORDER_WIDTH;
		break;
	case LAB_DECO_PART_RIGHT:
		box.x = view->x + view->surface->current.width;
		box.y = view->y - TITLE_HEIGHT;
		box.width = BORDER_WIDTH;
		box.height = view->surface->current.height + TITLE_HEIGHT;
		break;
	case LAB_DECO_PART_BOTTOM:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y + view->surface->current.height;
		box.width = view->surface->current.width + 2 * BORDER_WIDTH;
		box.height = +BORDER_WIDTH;
		break;
	case LAB_DECO_PART_LEFT:
		box.x = view->x - BORDER_WIDTH;
		box.y = view->y - TITLE_HEIGHT;
		box.width = BORDER_WIDTH;
		box.height = view->surface->current.height + TITLE_HEIGHT;
		break;
	default:
		break;
	}
	return box;
}

enum deco_part deco_at(struct view *view, double lx, double ly)
{
	enum deco_part deco_part;
	for (deco_part = 0; deco_part < LAB_DECO_NONE; ++deco_part) {
		struct wlr_box box = deco_box(view, deco_part);
		if (wlr_box_contains_point(&box, lx, ly))
			return deco_part;
	}
	return LAB_DECO_NONE;
}
