// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "desktop-state.h"
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "common/array.h"
#include "common/border.h"
#include "common/mem.h"
#include "labwc.h"
#include "output.h"
#include "ssd.h"
#include "view.h"

void
desktop_state_create(struct server *server, struct wl_list *desktop_outputs)
{
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct desktop_output *desktop_output = znew(*desktop_output);
		desktop_output->output = output;
		desktop_output->scale = output->wlr_output->scale;

		/*
		 * On output scale change, windows can move from one output to
		 * another, so we have to remember where they were.
		 */
		struct view *view;
		wl_array_init(&desktop_output->views);
		wl_list_for_each(view, &server->views, link) {
			if (view->output == output) {
				array_add(&desktop_output->views, view);
			}
		}

		wl_list_insert(desktop_outputs, &desktop_output->link);
	}
}

void desktop_state_destroy(struct wl_list *desktop_outputs)
{
	struct desktop_output *desktop_output, *next;
	wl_list_for_each_safe(desktop_output, next, desktop_outputs, link) {
		wl_list_remove(&desktop_output->link);
		wl_array_release(&desktop_output->views);
		zfree(desktop_output);
	}
}

static bool
get_scale(double *scale, struct output *output, struct wl_list *desktop_outputs)
{
	struct desktop_output *desktop_output;
	wl_list_for_each(desktop_output, desktop_outputs, link) {
		if (desktop_output->output == output) {
			*scale = desktop_output->scale;
			return true;
		}
	}
	assert(false && "output list mismatch");
	return false;
}

static void
fit_window_within_usable_area(struct view *view)
{
	struct wlr_box usable = output_usable_area_in_layout_coords(view->output);
	struct border margin = ssd_get_margin(view->ssd);

	/*
	 * This is quite a simple approach.
	 *
	 * We prioritise keeping windows big because with a scale increase, it
	 * is likely that users will want that. As such, we first move the
	 * window to the top/left corner and then shrink it to the usable area
	 * if needed.
	 */
	view_move(view, usable.x + margin.left, usable.y + margin.top);
	view_constrain_size_to_that_of_usable_area(view);
}

void
desktop_state_adjust_on_output_scale_change(struct server *server,
		struct wl_list *desktop_outputs)
{
	struct desktop_output *desktop_output;
	wl_list_for_each(desktop_output, desktop_outputs, link) {
		struct view **view;
		wl_array_for_each(view, &desktop_output->views) {
			struct output *output = desktop_output->output;
			double old_scale = 0.0;
			if (!get_scale(&old_scale, output, desktop_outputs)) {
				return;
			}
			/* Put window back on the pre-scale-change output */
			if ((*view)->output != output) {
				view_set_output(*view, output);
			}
			/* Adjust window position and size */
			if (old_scale != (*view)->output->wlr_output->scale) {
				fit_window_within_usable_area(*view);
			}
		}
	}
}
