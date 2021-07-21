#include <stdio.h>
#include <strings.h>
#include "labwc.h"
#include "ssd.h"

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

/*
 * view_wlr_output - return the output that a view is mostly on
 */
static struct wlr_output *
view_wlr_output(struct view *view)
{
	struct wlr_output_layout *layout = view->server->output_layout;
	struct wlr_output *output;

	/* TODO: make this a bit more sophisticated */
	output = wlr_output_layout_output_at(layout, view->x + view->w / 2,
					     view->y + view->h / 2);
	return output;
}

static struct output *
view_output(struct view *view)
{
	struct wlr_output *wlr_output = view_wlr_output(view);
	return output_from_wlr_output(view->server, wlr_output);
}

void
view_center(struct view *view)
{
	struct wlr_output *wlr_output = view_wlr_output(view);
	if (!wlr_output) {
		return;
	}

	struct wlr_output_layout *layout = view->server->output_layout;
	struct wlr_output_layout_output* ol_output =
		wlr_output_layout_get(layout, wlr_output);
	int center_x = ol_output->x + wlr_output->width / 2;
	int center_y = ol_output->y + wlr_output->height / 2;
	view_move(view, center_x - view->w / 2, center_y - view->h / 2);
}

static struct wlr_box
usable_area_in_layout_coords(struct output *output)
{
	struct wlr_box box = output->usable_area;
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	box.x -= ox;
	box.y -= oy;
	return box;
}

void
view_maximize(struct view *view, bool maximize)
{
	if (view->maximized == maximize) {
		return;
	}
	view->impl->maximize(view, maximize);
	if (maximize) {
		view->unmaximized_geometry.x = view->x;
		view->unmaximized_geometry.y = view->y;
		view->unmaximized_geometry.width = view->w;
		view->unmaximized_geometry.height = view->h;

		struct output *output = view_output(view);
		struct wlr_box box = usable_area_in_layout_coords(output);

		if (view->ssd.enabled) {
			struct border border = ssd_thickness(view);
			box.x += border.left;
			box.y += border.top;
			box.width -= border.right + border.left;
			box.height -= border.top + border.bottom;
		}
		box.width /= output->wlr_output->scale;
		box.height /= output->wlr_output->scale;
		view_move_resize(view, box);
		view->maximized = true;
	} else {
		/* unmaximize */
		view_move_resize(view, view->unmaximized_geometry);
		view->maximized = false;
	}
}

void
view_for_each_surface(struct view *view, wlr_surface_iterator_func_t iterator,
		void *user_data)
{
	view->impl->for_each_surface(view, iterator, user_data);
}

void
view_for_each_popup_surface(struct view *view, wlr_surface_iterator_func_t iterator,
		void *data)
{
	if (!view->impl->for_each_popup_surface) {
		return;
	}
	view->impl->for_each_popup_surface(view, iterator, data);
}

static struct border
view_border(struct view *view)
{
	struct border border = {
		.left = view->margin.left - view->padding.left,
		.top = view->margin.top - view->padding.top,
		.right = view->margin.right + view->padding.right,
		.bottom = view->margin.bottom + view->padding.bottom,
	};
	return border;
}

#define GAP (3)
void
view_move_to_edge(struct view *view, const char *direction)
{
	if (!view) {
		wlr_log(WLR_ERROR, "no view");
		return;
	}
	struct output *output = view_output(view);
	struct border border = view_border(view);
	struct wlr_box usable = usable_area_in_layout_coords(output);

	int x, y;
	if (!strcasecmp(direction, "left")) {
		x = usable.x + border.left + GAP;
		y = view->y;
	} else if (!strcasecmp(direction, "up")) {
		x = view->x;
		y = usable.y + border.top + GAP;
	} else if (!strcasecmp(direction, "right")) {
		x = usable.x + usable.width - view->w - border.right - GAP;
		y = view->y;
	} else if (!strcasecmp(direction, "down")) {
		x = view->x;
		y = usable.y + usable.height - view->h - border.bottom - GAP;
	}
	view_move(view, x, y);
}
