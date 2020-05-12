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
	if (!view)
		return box;
	switch (deco_part) {
	case LAB_DECO_PART_TOP:
		box.x = view->x - XWL_WINDOW_BORDER;
		box.y = view->y - XWL_TITLEBAR_HEIGHT - XWL_WINDOW_BORDER;
		box.width =
			view->surface->current.width + 2 * XWL_WINDOW_BORDER;
		box.height = XWL_TITLEBAR_HEIGHT + XWL_WINDOW_BORDER;
		break;
	default:
		break;
	}
	return box;
}

enum deco_part deco_at(struct view *view, double lx, double ly)
{
	struct wlr_box box;
	box = deco_box(view, LAB_DECO_PART_TOP);
	if (wlr_box_contains_point(&box, lx, ly))
		return LAB_DECO_PART_TOP;
	return LAB_DECO_NONE;
}
