#include "labwc.h"

struct wlr_box deco_max_extents(struct view *view)
{
	struct wlr_box box = {
		.x = view->x - XWL_WINDOW_BORDER,
		.y = view->y - XWL_TITLEBAR_HEIGHT - XWL_WINDOW_BORDER,
		.width = view->surface->current.width + 2 * XWL_WINDOW_BORDER,
		.height = view->surface->current.height + XWL_TITLEBAR_HEIGHT +
			  2 * XWL_WINDOW_BORDER,
	};
	return box;
}

struct wlr_box deco_box(struct view *view, enum deco_part deco_part)
{
	struct wlr_box box = { .x = 0, .y = 0, .width = 0, .height = 0 };
	if (!view || !view->surface)
		return box;
	switch (deco_part) {
	case LAB_DECO_PART_TITLE:
		box.x = view->x;
		box.y = view->y - XWL_TITLEBAR_HEIGHT;
		box.width = view->surface->current.width;
		box.height = XWL_TITLEBAR_HEIGHT;
		break;
	case LAB_DECO_PART_TOP:
		box.x = view->x - XWL_WINDOW_BORDER;
		box.y = view->y - XWL_TITLEBAR_HEIGHT - XWL_WINDOW_BORDER;
		box.width =
			view->surface->current.width + 2 * XWL_WINDOW_BORDER;
		box.height = + XWL_WINDOW_BORDER;
		break;
	case LAB_DECO_PART_RIGHT:
		box.x = view->x + view->surface->current.width;
		box.y = view->y - XWL_TITLEBAR_HEIGHT;
		box.width = XWL_WINDOW_BORDER;
		box.height = view->surface->current.height + XWL_TITLEBAR_HEIGHT;
		break;
	case LAB_DECO_PART_BOTTOM:
		box.x = view->x - XWL_WINDOW_BORDER;
		box.y = view->y + view->surface->current.height;
		box.width =
			view->surface->current.width + 2 * XWL_WINDOW_BORDER;
		box.height = + XWL_WINDOW_BORDER;
		break;
	case LAB_DECO_PART_LEFT:
		box.x = view->x - XWL_WINDOW_BORDER;
		box.y = view->y - XWL_TITLEBAR_HEIGHT;
		box.width = XWL_WINDOW_BORDER;
		box.height = view->surface->current.height + XWL_TITLEBAR_HEIGHT;
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
