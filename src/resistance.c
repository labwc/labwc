#include "labwc.h"
#include "config/rcxml.h"

struct edges {
	int left;
	int top;
	int right;
	int bottom;
};

static void
is_within_resistance_range(struct edges view, struct edges target,
		struct edges other, struct edges *flags, int strength)
{
	if (view.left >= other.left && target.left < other.left
			&& target.left >= other.left - strength) {
		 flags->left = 1;
	} else if (view.right <= other.right && target.right > other.right
			&& target.right <= other.right + strength) {
		flags->right = 1;
	}

	if (view.top >= other.top && target.top < other.top
			&& target.top >= other.top - strength) {
		flags->top = 1;
	} else if (view.bottom <= other.bottom && target.bottom > other.bottom
			&& target.bottom <= other.bottom + strength) {
		flags->bottom = 1;
	}
}

void
resistance_move_apply(struct view *view, double *x, double *y, bool screen_edge)
{
	struct server *server = view->server;
	struct wlr_box mgeom;
	struct output *output;
	struct border border = view_border(view);
	struct edges view_edges; /* The edges of the current view */
	struct edges target_edges; /* The desired edges */
	struct edges other_edges; /* The edges of the monitor/other view */
	struct edges flags; /* To be set in is_within_resistance_range() */

	view_edges.left = view->x - border.left - rc.gap;
	view_edges.top = view->y - border.top - rc.gap;
	view_edges.right = view->x + view->w + border.right + rc.gap;
	view_edges.bottom = view->y + view->h + border.bottom + rc.gap;

	target_edges.left = *x - border.left - rc.gap;
	target_edges.top = *y - border.top - rc.gap;
	target_edges.right = *x + view->w + border.right + rc.gap;
	target_edges.bottom = *y + view->h + border.bottom + rc.gap;

	if (screen_edge) {
		if (!rc.screen_edge_strength) {
			return;
		}

		wl_list_for_each(output, &server->outputs, link) {
			mgeom = output_usable_area_in_layout_coords(output);
	
			other_edges.left = mgeom.x;
			other_edges.top = mgeom.y;
			other_edges.right = mgeom.x + mgeom.width;
			other_edges.bottom = mgeom.y + mgeom.height;
	
			is_within_resistance_range(view_edges, target_edges,
				other_edges, &flags, rc.screen_edge_strength);

			if (flags.left == 1) {
				*x = other_edges.left + border.left + rc.gap;
			} else if (flags.right == 1) {
				*x = other_edges.right - view->w - border.right
					- rc.gap;
			}

			if (flags.top == 1) {
				*y = other_edges.top + border.top + rc.gap;
			} else if (flags.bottom == 1) {
				*y = other_edges.bottom - view->h
					- border.bottom - rc.gap;
			}

			/* reset the flags */
			flags.left = flags.top = flags.right = flags.bottom = 0;
		}
	}
}

void
resistance_resize_apply(struct view *view, struct wlr_box *new_view_geo,
		bool screen_edge)
{
	struct server *server = view->server;
	struct output *output;
	struct wlr_box mgeom;
	struct border border = view_border(view);
	struct edges view_edges; /* The edges of the current view */
	struct edges target_edges; /* The desired edges */
	struct edges other_edges; /* The edges of the monitor/other view */
	struct edges flags; /* To be set in is_within_resistance_range() */

	view_edges.left = view->x - border.left - rc.gap;
	view_edges.top = view->y - border.top - rc.gap;
	view_edges.right = view->x + view->w + border.right + rc.gap;
	view_edges.bottom = view->y + view->h + border.bottom + rc.gap;

	target_edges.left = new_view_geo->x - border.left - rc.gap;
	target_edges.top = new_view_geo->y - border.top - rc.gap;
	target_edges.right = new_view_geo->x + new_view_geo->width
		+ border.right + rc.gap;
	target_edges.bottom = new_view_geo->y + new_view_geo->height
		+ border.bottom + rc.gap;

	if (screen_edge) {
		if (!rc.screen_edge_strength) {
			return;
		}
		wl_list_for_each(output, &server->outputs, link) {
			mgeom = output_usable_area_in_layout_coords(output);
			other_edges.left = mgeom.x;
			other_edges.top = mgeom.y;
			other_edges.right = mgeom.x + mgeom.width;
			other_edges.bottom = mgeom.y + mgeom.height;

			is_within_resistance_range(view_edges, target_edges,
				other_edges, &flags, rc.screen_edge_strength);

			if (server->resize_edges & WLR_EDGE_LEFT) {
				if (flags.left == 1) {
					new_view_geo->x = other_edges.left
						+ border.left + rc.gap;
					new_view_geo->width = view->w;
				}
			} else if (server->resize_edges & WLR_EDGE_RIGHT) {
				if (flags.right == 1) {
					new_view_geo->width = other_edges.right
						- view_edges.left
						- (border.right + rc.gap) * 2 ;
				}
			}

			if (server->resize_edges & WLR_EDGE_TOP) {
				if (flags.top == 1) {
					new_view_geo->y = other_edges.top
						+ border.top + rc.gap;
					new_view_geo->height = view->h;
				}
			} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
				if (flags.bottom == 1) {
					new_view_geo->height =
						other_edges.bottom
						- view_edges.top - border.bottom
						- border.top - rc.gap * 2;
				}
			}
		}
	}
}
