// SPDX-License-Identifier: GPL-2.0-only
#include "show-desktop.h"
#include <wlr/types/wlr_seat.h>
#include "common/array.h"
#include "config/types.h"
#include "labwc.h"
#include "view.h"

static bool is_showing_desktop;

static void
minimize_views(struct wl_array *views, bool minimize)
{
	struct view **view;
	wl_array_for_each_reverse(view, views) {
		view_minimize(*view, minimize);
	}
}

static void
show(void)
{
	static struct wl_array views;
	wl_array_init(&views);

	/* Build array first as minimize changes server.views */
	struct view *view;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (view->minimized) {
			continue;
		}
		view->was_minimized_by_show_desktop_action = true;
		array_add(&views, view);
	}
	minimize_views(&views, true);
	is_showing_desktop = true;

	wl_array_release(&views);
}

static void
restore(void)
{
	static struct wl_array views;
	wl_array_init(&views);

	struct view *view;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (view->was_minimized_by_show_desktop_action) {
			array_add(&views, view);
		}
	}
	minimize_views(&views, false);
	show_desktop_reset();

	wl_array_release(&views);
}

void
show_desktop_toggle(void)
{
	if (is_showing_desktop) {
		restore();
	} else {
		show();
	}
}

void
show_desktop_reset(void)
{
	is_showing_desktop = false;

	struct view *view;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NONE) {
		view->was_minimized_by_show_desktop_action = false;
	}
}
