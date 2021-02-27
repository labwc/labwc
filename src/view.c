#include "labwc.h"

#include <stdio.h>
#include <assert.h>
void
view_move_resize(struct view *view, struct wlr_box geo)
{
	view->impl->configure(view, geo);
}

void
view_move(struct view *view, double x, double y)
{
	view->impl->move(view, x, y);
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
view_maximize(struct view *view, bool maximize)
{
	if(maximize == true)
	{
		struct wlr_output_layout *layout = view->server->output_layout;
		struct wlr_output* output =
			wlr_output_layout_output_at(layout, view->x, view->y);
		struct wlr_output_layout_output* ol_output =
			wlr_output_layout_get(layout, output);

		assert(layout);
		if(output == NULL)
		{
			die("output NULL w/ x and y of: %d, %d", view->x, view->y);
		}
		assert(output);
		assert(ol_output);

		int x = ol_output->x;
		int y = ol_output->y;
		int width = output->width;
		int height = output->height;

		if(view->server_side_deco)
		{
			struct border border = deco_thickness(view);
			x += border.right;
			x += border.left;
			y += border.top;
			y += border.bottom;

			width -= border.right;
			width -= border.left;
			height -= border.top;
			height -= border.bottom;
		}

		struct wlr_box box = {
			.x = x * output->scale,
			.y = y * output->scale,
			.width = width * output->scale,
			.height = height * output->scale
		};

		view_move_resize(view, box);
		view_move(view, box.x * output->scale, box.y * output->scale);

		view->maximized = true;
	}
	else
	{
		struct wlr_box box = {
			.x = 20,
			.y = 50,
			.width = 640,
			.height = 480
		};

		view_move_resize(view, box);
		view->maximized = false;
	}
	view->impl->maximize(view, maximize);
}

void
view_for_each_surface(struct view *view, wlr_surface_iterator_func_t iterator,
		void *user_data)
{
	view->impl->for_each_surface(view, iterator, user_data);
}

void
view_for_each_popup(struct view *view, wlr_surface_iterator_func_t iterator,
		void *data)
{
	if (!view->impl->for_each_popup) {
		return;
	}
	view->impl->for_each_popup(view, iterator, data);
}

