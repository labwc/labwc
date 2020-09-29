#include "labwc.h"

void
view_resize(struct view *view, struct wlr_box geo)
{
	struct wlr_box box = {
		.x = view->x,
		.y = view->y,
		.width = geo.width,
		.height = geo.height,
	};
	view->impl->configure(view, box);
}

void
view_minimize(struct view *view)
{
	if (view->minimized == true) {
		return;
	}
	view->minimized = true;
	view->impl->unmap(view);
}

void
view_unminimize(struct view *view)
{
	if (view->minimized == false) {
		return;
	}
	view->minimized = false;
	view->impl->map(view);
}

void
view_for_each_surface(struct view *view, wlr_surface_iterator_func_t iterator,
		void *user_data)
{
	view->impl->for_each_surface(view, iterator, user_data);
}

