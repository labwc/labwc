// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include "common/macros.h"
#include "common/match.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "menu/menu.h"
#include "placement.h"
#include "regions.h"
#include "resize_indicator.h"
#include "snap.h"
#include "ssd.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"
#include "xwayland.h"

#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif

#define LAB_FALLBACK_WIDTH  640
#define LAB_FALLBACK_HEIGHT 480

struct view *
view_from_wlr_surface(struct wlr_surface *surface)
{
	assert(surface);
	/*
	 * TODO:
	 * - find a way to get rid of xdg/xwayland-specific stuff
	 * - look up root/toplevel surface if passed a subsurface?
	 */
	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_try_from_wlr_surface(surface);
	if (xdg_surface) {
		return xdg_surface->data;
	}
#if HAVE_XWAYLAND
	struct wlr_xwayland_surface *xsurface =
		wlr_xwayland_surface_try_from_wlr_surface(surface);
	if (xsurface) {
		return xsurface->data;
	}
#endif
	return NULL;
}

void
view_query_free(struct view_query *query)
{
	wl_list_remove(&query->link);
	free(query->identifier);
	free(query->title);
	free(query);
}

bool
view_matches_query(struct view *view, struct view_query *query)
{
	bool match = true;
	bool empty = true;

	const char *identifier = view_get_string_prop(view, "app_id");
	if (match && query->identifier) {
		empty = false;
		match &= match_glob(query->identifier, identifier);
	}

	const char *title = view_get_string_prop(view, "title");
	if (match && query->title) {
		empty = false;
		match &= match_glob(query->title, title);
	}

	return !empty && match;
}

static bool
matches_criteria(struct view *view, enum lab_view_criteria criteria)
{
	if (!view_is_focusable(view)) {
		return false;
	}
	if (criteria & LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		/*
		 * Always-on-top views are always on the current desktop and are
		 * special in that they live in a different tree.
		 */
		struct server *server = view->server;
		if (view->scene_tree->node.parent != server->workspace_current->tree
				&& !view_is_always_on_top(view)) {
			return false;
		}
	}
	if (criteria & LAB_VIEW_CRITERIA_FULLSCREEN) {
		if (!view->fullscreen) {
			return false;
		}
	}
	if (criteria & LAB_VIEW_CRITERIA_ALWAYS_ON_TOP) {
		if (!view_is_always_on_top(view)) {
			return false;
		}
	}
	if (criteria & LAB_VIEW_CRITERIA_NO_ALWAYS_ON_TOP) {
		if (view_is_always_on_top(view)) {
			return false;
		}
	}
	if (criteria & LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER) {
		if (window_rules_get_property(view, "skipWindowSwitcher") == LAB_PROP_TRUE) {
			return false;
		}
	}
	return true;
}

struct view *
view_next(struct wl_list *head, struct view *view, enum lab_view_criteria criteria)
{
	assert(head);

	struct wl_list *elm = view ? &view->link : head;

	for (elm = elm->next; elm != head; elm = elm->next) {
		view = wl_container_of(elm, view, link);
		if (matches_criteria(view, criteria)) {
			return view;
		}
	}
	return NULL;
}

void
view_array_append(struct server *server, struct wl_array *views,
		enum lab_view_criteria criteria)
{
	struct view *view;
	for_each_view(view, &server->views, criteria) {
		struct view **entry = wl_array_add(views, sizeof(*entry));
		if (!entry) {
			wlr_log(WLR_ERROR, "wl_array_add(): out of memory");
			continue;
		}
		*entry = view;
	}
}

enum view_wants_focus
view_wants_focus(struct view *view)
{
	assert(view);
	if (view->impl->wants_focus) {
		return view->impl->wants_focus(view);
	}
	return VIEW_WANTS_FOCUS_ALWAYS;
}

bool
view_is_focusable_from(struct view *view, struct wlr_surface *prev)
{
	assert(view);
	if (!view->surface) {
		return false;
	}
	if (!view->mapped && !view->minimized) {
		return false;
	}
	enum view_wants_focus wants_focus = view_wants_focus(view);
	/*
	 * Consider "offer focus" (Globally Active) views as focusable
	 * only if another surface from the same application already had
	 * focus. The goal is to allow focusing a parent window when a
	 * dialog/popup is closed, but still avoid focusing standalone
	 * panels/toolbars/notifications. Note that we are basically
	 * guessing whether Globally Active views want focus, and will
	 * probably be wrong some of the time.
	 */
	return (wants_focus == VIEW_WANTS_FOCUS_ALWAYS
		|| (wants_focus == VIEW_WANTS_FOCUS_OFFER
			&& prev && view_is_related(view, prev)));
}

/**
 * All view_apply_xxx_geometry() functions must *not* modify
 * any state besides repositioning or resizing the view.
 *
 * They may be called repeatably during output layout changes.
 */

static enum view_edge
view_edge_invert(enum view_edge edge)
{
	switch (edge) {
	case VIEW_EDGE_LEFT:
		return VIEW_EDGE_RIGHT;
	case VIEW_EDGE_RIGHT:
		return VIEW_EDGE_LEFT;
	case VIEW_EDGE_UP:
		return VIEW_EDGE_DOWN;
	case VIEW_EDGE_DOWN:
		return VIEW_EDGE_UP;
	case VIEW_EDGE_CENTER:
	case VIEW_EDGE_INVALID:
	default:
		return VIEW_EDGE_INVALID;
	}
}

static struct wlr_box
view_get_edge_snap_box(struct view *view, struct output *output,
		enum view_edge edge)
{
	struct wlr_box usable = output_usable_area_scaled(output);
	int x_offset = edge == VIEW_EDGE_RIGHT
		? (usable.width + rc.gap) / 2 : rc.gap;
	int y_offset = edge == VIEW_EDGE_DOWN
		? (usable.height + rc.gap) / 2 : rc.gap;

	int base_width, base_height;
	switch (edge) {
	case VIEW_EDGE_LEFT:
	case VIEW_EDGE_RIGHT:
		base_width = (usable.width - 3 * rc.gap) / 2;
		base_height = usable.height - 2 * rc.gap;
		break;
	case VIEW_EDGE_UP:
	case VIEW_EDGE_DOWN:
		base_width = usable.width - 2 * rc.gap;
		base_height = (usable.height - 3 * rc.gap) / 2;
		break;
	default:
	case VIEW_EDGE_CENTER:
		base_width = usable.width - 2 * rc.gap;
		base_height = usable.height - 2 * rc.gap;
		break;
	}

	struct border margin = ssd_get_margin(view->ssd);
	struct wlr_box dst = {
		.x = x_offset + usable.x + margin.left,
		.y = y_offset + usable.y + margin.top,
		.width = base_width - margin.left - margin.right,
		.height = base_height - margin.top - margin.bottom,
	};

	return dst;
}

static bool
view_discover_output(struct view *view, struct wlr_box *geometry)
{
	assert(view);
	assert(!view->fullscreen);

	if (!geometry) {
		geometry = &view->current;
	}

	struct output *output =
		output_nearest_to(view->server,
			geometry->x + geometry->width / 2,
			geometry->y + geometry->height / 2);

	if (output && output != view->output) {
		view->output = output;
		return true;
	}

	return false;
}

static void
set_adaptive_sync_fullscreen(struct view *view)
{
	if (rc.adaptive_sync != LAB_ADAPTIVE_SYNC_FULLSCREEN) {
		return;
	}
	/* Enable adaptive sync if view is fullscreen */
	output_enable_adaptive_sync(view->output->wlr_output, view->fullscreen);
	wlr_output_commit(view->output->wlr_output);
}

void
view_set_activated(struct view *view, bool activated)
{
	assert(view);
	ssd_set_active(view->ssd, activated);
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}
	if (view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_set_activated(
			view->toplevel.handle, activated);
	}

	if (rc.kb_layout_per_window) {
		if (!activated) {
			/* Store configured keyboard layout per view */
			view->keyboard_layout =
				view->server->seat.keyboard_group->keyboard.modifiers.group;
		} else {
			/* Switch to previously stored keyboard layout */
			keyboard_update_layout(&view->server->seat, view->keyboard_layout);
		}
	}
	set_adaptive_sync_fullscreen(view);
}

void
view_set_output(struct view *view, struct output *output)
{
	assert(view);
	assert(!view->fullscreen);
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "invalid output set for view");
		return;
	}
	view->output = output;
}

void
view_close(struct view *view)
{
	assert(view);
	if (view->impl->close) {
		view->impl->close(view);
	}
}

void
view_move(struct view *view, int x, int y)
{
	assert(view);
	view_move_resize(view, (struct wlr_box){
		.x = x, .y = y,
		.width = view->pending.width,
		.height = view->pending.height
	});
}

void
view_moved(struct view *view)
{
	assert(view);
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->current.x, view->current.y);
	/*
	 * Only floating views change output when moved. Non-floating
	 * views (maximized/tiled/fullscreen) are tied to a particular
	 * output when they enter that state.
	 */
	if (view_is_floating(view)) {
		view_discover_output(view, NULL);
	}
	ssd_update_geometry(view->ssd);
	cursor_update_focus(view->server);
	if (view->toplevel.handle) {
		foreign_toplevel_update_outputs(view);
	}
	if (rc.resize_indicator && view->server->grabbed_view == view) {
		resize_indicator_update(view);
	}
}

void
view_move_resize(struct view *view, struct wlr_box geo)
{
	assert(view);
	if (view->impl->configure) {
		view->impl->configure(view, geo);
	}
}

void
view_resize_relative(struct view *view, int left, int right, int top, int bottom)
{
	assert(view);
	if (view->fullscreen || view->maximized != VIEW_AXIS_NONE) {
		return;
	}
	struct wlr_box newgeo = view->pending;
	newgeo.x -= left;
	newgeo.width += left + right;
	newgeo.y -= top;
	newgeo.height += top + bottom;
	view_move_resize(view, newgeo);
	view_set_untiled(view);
}

void
view_move_relative(struct view *view, int x, int y)
{
	assert(view);
	if (view->fullscreen) {
		return;
	}
	view_maximize(view, VIEW_AXIS_NONE, /*store_natural_geometry*/ false);
	if (view_is_tiled(view)) {
		view_set_untiled(view);
		view_restore_to(view, view->natural_geometry);
	}
	view_move(view, view->pending.x + x, view->pending.y + y);
}

void
view_move_to_cursor(struct view *view)
{
	assert(view);

	struct output *pending_output = output_nearest_to_cursor(view->server);
	if (!output_is_usable(pending_output)) {
		return;
	}
	view_set_fullscreen(view, false);
	view_maximize(view, VIEW_AXIS_NONE, /*store_natural_geometry*/ false);
	if (view_is_tiled(view)) {
		view_set_untiled(view);
		view_restore_to(view, view->natural_geometry);
	}

	struct border margin = ssd_thickness(view);
	struct wlr_box geo = view->pending;
	geo.width += margin.left + margin.right;
	geo.height += margin.top + margin.bottom;

	int x = view->server->seat.cursor->x - (geo.width / 2);
	int y = view->server->seat.cursor->y - (geo.height / 2);

	struct wlr_box usable = output_usable_area_in_layout_coords(pending_output);
	if (x + geo.width > usable.x + usable.width) {
		x = usable.x + usable.width - geo.width;
	}
	x = MAX(x, usable.x);

	if (y + geo.height > usable.y + usable.height) {
		y = usable.y + usable.height - geo.height;
	}
	y = MAX(y, usable.y);

	x += margin.left;
	y += margin.top;
	view_move(view, x, y);
}

struct view_size_hints
view_get_size_hints(struct view *view)
{
	assert(view);
	if (view->impl->get_size_hints) {
		return view->impl->get_size_hints(view);
	}
	return (struct view_size_hints){0};
}

static void
substitute_nonzero(int *a, int *b)
{
	if (!(*a)) {
		*a = *b;
	} else if (!(*b)) {
		*b = *a;
	}
}

static int
round_to_increment(int val, int base, int inc)
{
	if (base < 0 || inc <= 0) {
		return val;
	}
	return base + (val - base + inc / 2) / inc * inc;
}

void
view_adjust_size(struct view *view, int *w, int *h)
{
	assert(view);
	struct view_size_hints hints = view_get_size_hints(view);

	/*
	 * "If a base size is not provided, the minimum size is to be
	 * used in its place and vice versa." (ICCCM 4.1.2.3)
	 */
	substitute_nonzero(&hints.min_width, &hints.base_width);
	substitute_nonzero(&hints.min_height, &hints.base_height);

	/*
	 * Snap width/height to requested size increments (if any).
	 * Typically, terminal emulators use these to make sure that the
	 * terminal is resized to a width/height evenly divisible by the
	 * cell (character) size.
	 */
	*w = round_to_increment(*w, hints.base_width, hints.width_inc);
	*h = round_to_increment(*h, hints.base_height, hints.height_inc);

	/*
	 * If a minimum width/height was not set, then use default.
	 * This is currently always the case for xdg-shell views.
	 */
	if (hints.min_width < 1) {
		hints.min_width = LAB_MIN_VIEW_WIDTH;
	}
	if (hints.min_height < 1) {
		hints.min_height = LAB_MIN_VIEW_HEIGHT;
	}
	*w = MAX(*w, hints.min_width);
	*h = MAX(*h, hints.min_height);
}

static void
_minimize(struct view *view, bool minimized)
{
	assert(view);
	if (view->minimized == minimized) {
		return;
	}
	if (view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_set_minimized(
			view->toplevel.handle, minimized);
	}
	if (view->impl->minimize) {
		view->impl->minimize(view, minimized);
	}
	view->minimized = minimized;
	if (minimized) {
		view->impl->unmap(view, /* client_request */ false);
	} else {
		view->impl->map(view);
	}
}

static void
minimize_sub_views(struct view *view, bool minimized)
{
	struct view **child;
	struct wl_array children;

	wl_array_init(&children);
	view_append_children(view, &children);
	wl_array_for_each(child, &children) {
		_minimize(*child, minimized);
		minimize_sub_views(*child, minimized);
	}
	wl_array_release(&children);
}

/*
 * Minimize the whole view-hierarchy from top to bottom regardless of which one
 * in the hierarchy requested the minimize. For example, if an 'About' or
 * 'Open File' dialog is minimized, its toplevel is minimized also. And vice
 * versa.
 */
void
view_minimize(struct view *view, bool minimized)
{
	assert(view);
	/*
	 * Minimize the root window first because some xwayland clients send a
	 * request-unmap to sub-windows at this point (for example gimp and its
	 * 'open file' dialog), so it saves trying to unmap them twice
	 */
	struct view *root = view_get_root(view);
	_minimize(root, minimized);
	minimize_sub_views(root, minimized);
}

bool
view_compute_centered_position(struct view *view, const struct wlr_box *ref,
		int w, int h, int *x, int *y)
{
	assert(view);
	if (w <= 0 || h <= 0) {
		wlr_log(WLR_ERROR, "view has empty geometry, not centering");
		return false;
	}
	if (!output_is_usable(view->output)) {
		wlr_log(WLR_ERROR, "view has no output, not centering");
		return false;
	}

	struct border margin = ssd_get_margin(view->ssd);
	struct wlr_box usable = output_usable_area_in_layout_coords(view->output);
	int width = w + margin.left + margin.right;
	int height = h + margin.top + margin.bottom;

	/* If reference box is NULL then center to usable area */
	if (!ref) {
		ref = &usable;
	}
	*x = ref->x + (ref->width - width) / 2;
	*y = ref->y + (ref->height - height) / 2;

	/* If view is bigger than usable area, just top/left align it */
	if (*x < usable.x) {
		*x = usable.x;
	}
	if (*y < usable.y) {
		*y = usable.y;
	}

	*x += margin.left;
	*y += margin.top;

	return true;
}

bool
view_adjust_floating_geometry(struct view *view, struct wlr_box *geometry)
{
	assert(view);
	if (!output_is_usable(view->output)) {
		wlr_log(WLR_ERROR, "view has no output, not positioning");
		return false;
	}

	/* Avoid moving panels out of their own reserved area ("strut") */
	if (window_rules_get_property(view, "fixedPosition") == LAB_PROP_TRUE
			|| view_has_strut_partial(view)) {
		return false;
	}

	bool adjusted = false;
	/*
	 * First check whether the view is the target screen, meaning that at
	 * least one client pixel is on the screen.
	 */
	if (wlr_output_layout_intersects(view->server->output_layout,
			view->output->wlr_output, geometry)) {
		/*
		 * If onscreen, then make sure the titlebar is also
		 * visible (and not overlapping any panels/docks)
		 */
		struct border margin = ssd_get_margin(view->ssd);
		struct wlr_box usable =
			output_usable_area_in_layout_coords(view->output);

		if (geometry->x < usable.x + margin.left) {
			geometry->x = usable.x + margin.left;
			adjusted = true;
		}
		if (geometry->y < usable.y + margin.top) {
			geometry->y = usable.y + margin.top;
			adjusted = true;
		}
	} else {
		/*
		 * Reposition offscreen views; if automatic placement was is
		 * configured, try to automatically place the windows.
		 */
		if (rc.placement_policy == LAB_PLACE_AUTOMATIC) {
			if (placement_find_best(view, geometry)) {
				return true;
			}
		}

		/* If automatic placement failed or was not enabled, just center */
		adjusted = view_compute_centered_position(view, NULL,
			geometry->width, geometry->height,
			&geometry->x, &geometry->y);
	}
	return adjusted;
}

static void
set_fallback_geometry(struct view *view)
{
	view->natural_geometry.width = LAB_FALLBACK_WIDTH;
	view->natural_geometry.height = LAB_FALLBACK_HEIGHT;
	view_compute_centered_position(view, NULL,
		view->natural_geometry.width,
		view->natural_geometry.height,
		&view->natural_geometry.x,
		&view->natural_geometry.y);
}

#undef LAB_FALLBACK_WIDTH
#undef LAB_FALLBACK_HEIGHT

void
view_store_natural_geometry(struct view *view)
{
	assert(view);
	if (!view_is_floating(view)) {
		/* Do not overwrite the stored geometry with special cases */
		return;
	}

	/**
	 * If an application was started maximized or fullscreened, its
	 * natural_geometry width/height may still be zero in which case we set
	 * some fallback values. This is the case with foot and Qt applications.
	 */
	if (wlr_box_empty(&view->pending)) {
		set_fallback_geometry(view);
	} else {
		view->natural_geometry = view->pending;
	}
}

void
view_center(struct view *view, const struct wlr_box *ref)
{
	assert(view);
	int x, y;
	if (view_compute_centered_position(view, ref, view->pending.width,
			view->pending.height, &x, &y)) {
		view_move(view, x, y);
	}
}

void
view_place_initial(struct view *view)
{
	if (rc.placement_policy == LAB_PLACE_CURSOR) {
		view_move_to_cursor(view);
		return;
	} else if (rc.placement_policy == LAB_PLACE_AUTOMATIC) {
		struct wlr_box geometry = view->pending;
		if (placement_find_best(view, &geometry)) {
			view_move(view, geometry.x, geometry.y);
			return;
		}
	}

	view_center(view, NULL);
}

static void
view_apply_natural_geometry(struct view *view)
{
	assert(view);
	assert(view_is_floating(view));

	struct wlr_box geometry = view->natural_geometry;
	view_adjust_floating_geometry(view, &geometry);
	view_move_resize(view, geometry);
}

static void
view_apply_region_geometry(struct view *view)
{
	assert(view);
	assert(view->tiled_region || view->tiled_region_evacuate);
	struct output *output = view->output;
	assert(output_is_usable(output));

	if (view->tiled_region_evacuate) {
		/* View was evacuated from a destroying output */
		/* Get new output local region, may be NULL */
		view->tiled_region = regions_from_name(
			view->tiled_region_evacuate, output);

		/* Get rid of the evacuate instruction */
		zfree(view->tiled_region_evacuate);

		if (!view->tiled_region) {
			/* Existing region name doesn't exist in rc.xml anymore */
			view_set_untiled(view);
			view_apply_natural_geometry(view);
			return;
		}
	}

	/* Create a copy of the original region geometry */
	struct wlr_box geo = view->tiled_region->geo;

	/* Adjust for rc.gap */
	if (rc.gap) {
		double half_gap = rc.gap / 2.0;
		struct wlr_fbox offset = {
			.x = half_gap,
			.y = half_gap,
			.width = -rc.gap,
			.height = -rc.gap
		};
		struct wlr_box usable =
			output_usable_area_in_layout_coords(output);
		if (geo.x == usable.x) {
			offset.x += half_gap;
			offset.width -= half_gap;
		}
		if (geo.y == usable.y) {
			offset.y += half_gap;
			offset.height -= half_gap;
		}
		if (geo.x + geo.width == usable.x + usable.width) {
			offset.width -= half_gap;
		}
		if (geo.y + geo.height == usable.y + usable.height) {
			offset.height -= half_gap;
		}
		geo.x += offset.x;
		geo.y += offset.y;
		geo.width += offset.width;
		geo.height += offset.height;
	}

	/* And adjust for current view */
	struct border margin = ssd_get_margin(view->ssd);
	geo.x += margin.left;
	geo.y += margin.top;
	geo.width -= margin.left + margin.right;
	geo.height -= margin.top + margin.bottom;

	view_move_resize(view, geo);
}

static void
view_apply_tiled_geometry(struct view *view)
{
	assert(view);
	assert(view->tiled);
	assert(output_is_usable(view->output));

	view_move_resize(view, view_get_edge_snap_box(view,
		view->output, view->tiled));
}

static void
view_apply_fullscreen_geometry(struct view *view)
{
	assert(view);
	assert(view->fullscreen);
	assert(output_is_usable(view->output));

	struct wlr_box box = { 0 };
	wlr_output_effective_resolution(view->output->wlr_output,
		&box.width, &box.height);
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(view->server->output_layout,
		view->output->wlr_output, &ox, &oy);
	box.x -= ox;
	box.y -= oy;
	view_move_resize(view, box);
}

static void
view_apply_maximized_geometry(struct view *view)
{
	assert(view);
	assert(view->maximized != VIEW_AXIS_NONE);
	struct output *output = view->output;
	assert(output_is_usable(output));

	struct wlr_box box = output_usable_area_in_layout_coords(output);
	if (box.height == output->wlr_output->height
			&& output->wlr_output->scale != 1) {
		box.height /= output->wlr_output->scale;
	}
	if (box.width == output->wlr_output->width
			&& output->wlr_output->scale != 1) {
		box.width /= output->wlr_output->scale;
	}

	/*
	 * If one axis (horizontal or vertical) is unmaximized, it
	 * should use the natural geometry. But if that geometry is not
	 * on-screen on the output where the view is maximized, then
	 * center the unmaximized axis.
	 */
	struct wlr_box natural = view->natural_geometry;
	if (view->maximized != VIEW_AXIS_BOTH) {
		struct wlr_box intersect;
		wlr_box_intersection(&intersect, &box, &natural);
		if (wlr_box_empty(&intersect)) {
			view_compute_centered_position(view, NULL,
				natural.width, natural.height,
				&natural.x, &natural.y);
		}
	}

	if (view->ssd_enabled) {
		struct border border = ssd_thickness(view);
		box.x += border.left;
		box.y += border.top;
		box.width -= border.right + border.left;
		box.height -= border.top + border.bottom;
	}

	if (view->maximized == VIEW_AXIS_VERTICAL) {
		box.x = natural.x;
		box.width = natural.width;
	} else if (view->maximized == VIEW_AXIS_HORIZONTAL) {
		box.y = natural.y;
		box.height = natural.height;
	}

	view_move_resize(view, box);
}

static void
view_apply_special_geometry(struct view *view)
{
	assert(view);
	assert(!view_is_floating(view));
	if (!output_is_usable(view->output)) {
		wlr_log(WLR_ERROR, "view has no output, not updating geometry");
		return;
	}

	if (view->fullscreen) {
		view_apply_fullscreen_geometry(view);
	} else if (view->maximized != VIEW_AXIS_NONE) {
		view_apply_maximized_geometry(view);
	} else if (view->tiled) {
		view_apply_tiled_geometry(view);
	} else if (view->tiled_region || view->tiled_region_evacuate) {
		view_apply_region_geometry(view);
	} else {
		assert(false); // not reached
	}
}

/* For internal use only. Does not update geometry. */
static void
set_maximized(struct view *view, enum view_axis maximized)
{
	if (view->impl->maximize) {
		view->impl->maximize(view, (maximized == VIEW_AXIS_BOTH));
	}
	if (view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_set_maximized(
			view->toplevel.handle, (maximized == VIEW_AXIS_BOTH));
	}
	view->maximized = maximized;

	/*
	 * Ensure that follow-up actions like SnapToEdge / SnapToRegion
	 * use up-to-date SSD margin information. Otherwise we will end
	 * up using an outdated ssd->margin to calculate offsets.
	 */
	ssd_update_margin(view->ssd);
}

/*
 * Un-maximize view and move it to specific geometry. Does not reset
 * tiled state (use view_set_untiled() if you want that).
 */
void
view_restore_to(struct view *view, struct wlr_box geometry)
{
	assert(view);
	if (view->fullscreen) {
		return;
	}
	if (view->maximized != VIEW_AXIS_NONE) {
		set_maximized(view, VIEW_AXIS_NONE);
	}
	view_move_resize(view, geometry);
}

bool
view_is_tiled(struct view *view)
{
	assert(view);
	return (view->tiled || view->tiled_region
		|| view->tiled_region_evacuate);
}

bool
view_is_floating(struct view *view)
{
	assert(view);
	return !(view->fullscreen || (view->maximized != VIEW_AXIS_NONE)
		|| view_is_tiled(view));
}

/* Reset tiled state of view without changing geometry */
void
view_set_untiled(struct view *view)
{
	assert(view);
	view->tiled = VIEW_EDGE_INVALID;
	view->tiled_region = NULL;
	zfree(view->tiled_region_evacuate);
}

void
view_maximize(struct view *view, enum view_axis axis,
		bool store_natural_geometry)
{
	assert(view);
	if (view->maximized == axis) {
		return;
	}
	if (view->fullscreen) {
		return;
	}
	if (axis != VIEW_AXIS_NONE) {
		/*
		 * Maximize via keybind or client request cancels
		 * interactive move/resize since we can't move/resize
		 * a maximized view.
		 */
		interactive_cancel(view);
		if (store_natural_geometry && view_is_floating(view)) {
			view_store_natural_geometry(view);
			view_invalidate_last_layout_geometry(view);
		}
	}
	set_maximized(view, axis);
	if (view_is_floating(view)) {
		view_apply_natural_geometry(view);
	} else {
		view_apply_special_geometry(view);
	}
}

void
view_toggle_maximize(struct view *view, enum view_axis axis)
{
	assert(view);
	switch (axis) {
	case VIEW_AXIS_HORIZONTAL:
	case VIEW_AXIS_VERTICAL:
		/* Toggle one axis (XOR) */
		view_maximize(view, view->maximized ^ axis,
			/*store_natural_geometry*/ true);
		break;
	case VIEW_AXIS_BOTH:
		/*
		 * Maximize in both directions if unmaximized or partially
		 * maximized, otherwise unmaximize.
		 */
		view_maximize(view, (view->maximized == VIEW_AXIS_BOTH) ?
			VIEW_AXIS_NONE : VIEW_AXIS_BOTH,
			/*store_natural_geometry*/ true);
		break;
	default:
		break;
	}
}

void
view_toggle_decorations(struct view *view)
{
	assert(view);
	if (rc.ssd_keep_border && view->ssd_enabled && view->ssd
			&& !view->ssd_titlebar_hidden) {
		/*
		 * ssd_titlebar_hidden has to be set before calling
		 * ssd_titlebar_hide() to make ssd_thickness() happy.
		 */
		view->ssd_titlebar_hidden = true;
		ssd_titlebar_hide(view->ssd);
		if (!view_is_floating(view)) {
			view_apply_special_geometry(view);
		}
		return;
	}
	view_set_decorations(view, !view->ssd_enabled);
}

bool
view_is_always_on_top(struct view *view)
{
	assert(view);
	return view->scene_tree->node.parent ==
		view->server->view_tree_always_on_top;
}

void
view_toggle_always_on_top(struct view *view)
{
	assert(view);
	if (view_is_always_on_top(view)) {
		view->workspace = view->server->workspace_current;
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->workspace->tree);
	} else {
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->server->view_tree_always_on_top);
	}
}

bool
view_is_always_on_bottom(struct view *view)
{
	assert(view);
	return view->scene_tree->node.parent ==
		view->server->view_tree_always_on_bottom;
}

void
view_toggle_always_on_bottom(struct view *view)
{
	assert(view);
	if (view_is_always_on_bottom(view)) {
		view->workspace = view->server->workspace_current;
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->workspace->tree);
	} else {
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->server->view_tree_always_on_bottom);
	}
}

void
view_toggle_visible_on_all_workspaces(struct view *view)
{
	assert(view);
	view->visible_on_all_workspaces = !view->visible_on_all_workspaces;
}

void
view_move_to_workspace(struct view *view, struct workspace *workspace)
{
	assert(view);
	assert(workspace);
	if (view->workspace != workspace) {
		view->workspace = workspace;
		wlr_scene_node_reparent(&view->scene_tree->node,
			workspace->tree);
	}
}

static void
decorate(struct view *view)
{
	if (!view->ssd) {
		view->ssd = ssd_create(view,
			view == view->server->active_view);
	}
}

static void
undecorate(struct view *view)
{
	ssd_destroy(view->ssd);
	view->ssd = NULL;
}

void
view_set_decorations(struct view *view, bool decorations)
{
	assert(view);

	if (view->ssd_enabled != decorations && !view->fullscreen) {
		/*
		 * Set view->ssd_enabled first since it is referenced
		 * within the call tree of ssd_create()
		 */
		view->ssd_enabled = decorations;
		if (decorations) {
			decorate(view);
		} else {
			undecorate(view);
			view->ssd_titlebar_hidden = false;
		}
		if (!view_is_floating(view)) {
			view_apply_special_geometry(view);
		}
	}
}

void
view_toggle_fullscreen(struct view *view)
{
	assert(view);
	view_set_fullscreen(view, !view->fullscreen);
}

/* For internal use only. Does not update geometry. */
static void
set_fullscreen(struct view *view, bool fullscreen)
{
	/* Hide decorations when going fullscreen */
	if (fullscreen && view->ssd_enabled) {
		undecorate(view);
	}

	if (view->impl->set_fullscreen) {
		view->impl->set_fullscreen(view, fullscreen);
	}
	if (view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_set_fullscreen(
			view->toplevel.handle, fullscreen);
	}
	view->fullscreen = fullscreen;

	/* Re-show decorations when no longer fullscreen */
	if (!fullscreen && view->ssd_enabled) {
		decorate(view);
	}

	/* Show fullscreen views above top-layer */
	if (view->output) {
		desktop_update_top_layer_visiblity(view->server);
	}
}

void
view_set_fullscreen(struct view *view, bool fullscreen)
{
	assert(view);
	if (fullscreen == view->fullscreen) {
		return;
	}
	if (fullscreen) {
		if (!output_is_usable(view->output)) {
			/* Prevent fullscreen with no available outputs */
			return;
		}
		/*
		 * Fullscreen via keybind or client request cancels
		 * interactive move/resize since we can't move/resize
		 * a fullscreen view.
		 */
		interactive_cancel(view);
		view_store_natural_geometry(view);
		view_invalidate_last_layout_geometry(view);
	}

	set_fullscreen(view, fullscreen);
	if (view_is_floating(view)) {
		view_apply_natural_geometry(view);
	} else {
		view_apply_special_geometry(view);
	}
	set_adaptive_sync_fullscreen(view);
}

static bool
last_layout_geometry_is_valid(struct view *view)
{
	return view->last_layout_geometry.width > 0
		&& view->last_layout_geometry.height > 0;
}

static void
update_last_layout_geometry(struct view *view)
{
	/*
	 * Only update an invalid last-layout geometry to prevent a series of
	 * successive layout changes from continually replacing the "preferred"
	 * location with whatever location the view currently holds. The
	 * "preferred" location should be whatever state was set by user
	 * interaction, not automatic responses to layout changes.
	 */
	if (last_layout_geometry_is_valid(view)) {
		return;
	}

	if (view_is_floating(view)) {
		view->last_layout_geometry = view->pending;
	} else {
		view->last_layout_geometry = view->natural_geometry;
	}
}

static bool
apply_last_layout_geometry(struct view *view, bool force_update)
{
	/* Only apply a valid last-layout geometry */
	if (!last_layout_geometry_is_valid(view)) {
		return false;
	}

	/*
	 * Unless forced, the last-layout geometry is only applied
	 * when the relevant view geometry is distinct.
	 */
	if (!force_update) {
		struct wlr_box *relevant = view_is_floating(view) ?
			&view->pending : &view->natural_geometry;

		if (wlr_box_equal(relevant, &view->last_layout_geometry)) {
			return false;
		}
	}

	view->natural_geometry = view->last_layout_geometry;
	view_adjust_floating_geometry(view, &view->natural_geometry);
	return true;
}

void
view_invalidate_last_layout_geometry(struct view *view)
{
	assert(view);
	view->last_layout_geometry.width = 0;
	view->last_layout_geometry.height = 0;
}

void
view_adjust_for_layout_change(struct view *view)
{
	assert(view);

	bool was_fullscreen = view->fullscreen;
	bool is_floating = view_is_floating(view);

	if (!output_is_usable(view->output)) {
		/* A view losing an output should have a last-layout geometry */
		update_last_layout_geometry(view);

		/* Exit fullscreen and re-assess floating status */
		if (was_fullscreen) {
			set_fullscreen(view, false);
			is_floating = view_is_floating(view);
		}
	}

	/* Restore any full-screen window to natural geometry */
	bool use_natural = was_fullscreen;

	/* Capture a pointer to the last-layout geometry (only if valid) */
	struct wlr_box *last_geometry = NULL;
	if (last_layout_geometry_is_valid(view)) {
		last_geometry = &view->last_layout_geometry;
	}

	/*
	 * Check if an output change is required:
	 * - Floating views are always mapped to the nearest output
	 * - Any view without a usable output needs to be repositioned
	 * - Any view with a valid last-layout geometry might be better
	 *   positioned on another output
	 */
	if (is_floating || last_geometry || !output_is_usable(view->output)) {
		/* Move the view to an appropriate output, if needed */
		bool output_changed = view_discover_output(view, last_geometry);

		/*
		 * Try to apply the last-layout to the natural geometry
		 * (adjusting to ensure that it fits on the screen). This is
		 * forced if the output has changed, but will be done
		 * opportunistically even on the same output if the last-layout
		 * geometry is different from the view's governing geometry.
		 */
		if (apply_last_layout_geometry(view, output_changed)) {
			use_natural = true;
		}

		/*
		 * Whether or not the view has moved, the layout has changed.
		 * Ensure that the view now has a valid last-layout geometry.
		 */
		update_last_layout_geometry(view);
	}

	if (!is_floating) {
		view_apply_special_geometry(view);
	} else if (use_natural) {
		/*
		 * Move the window to its natural location, either because it
		 * was fullscreen or we are trying to restore a prior layout.
		 */
		view_apply_natural_geometry(view);
	} else {
		/* Otherwise, just ensure the view is on screen. */
		struct wlr_box geometry = view->pending;
		if (view_adjust_floating_geometry(view, &geometry)) {
			view_move_resize(view, geometry);
		}
	}

	if (view->toplevel.handle) {
		foreign_toplevel_update_outputs(view);
	}
}

void
view_evacuate_region(struct view *view)
{
	assert(view);
	assert(view->tiled_region);
	if (!view->tiled_region_evacuate) {
		view->tiled_region_evacuate = xstrdup(view->tiled_region->name);
	}
	view->tiled_region = NULL;
}

void
view_on_output_destroy(struct view *view)
{
	assert(view);
	/*
	 * This is the only time we modify view->output for a fullscreen
	 * view. We expect view_adjust_for_layout_change() to be called
	 * shortly afterward, which will exit fullscreen.
	 */
	view->output = NULL;
}

static struct output *
view_get_adjacent_output(struct view *view, enum view_edge edge)
{
	assert(view);
	struct output *output = view->output;
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR,
			"view has no output, cannot find adjacent output");
		return NULL;
	}

	/* Determine any adjacent output in the appropriate direction */
	struct wlr_output *new_output = NULL;
	struct wlr_output *current_output = output->wlr_output;
	struct wlr_output_layout *layout = view->server->output_layout;
	switch (edge) {
	case VIEW_EDGE_LEFT:
		new_output = wlr_output_layout_adjacent_output(
			layout, WLR_DIRECTION_LEFT, current_output, 1, 0);
		break;
	case VIEW_EDGE_RIGHT:
		new_output = wlr_output_layout_adjacent_output(
			layout, WLR_DIRECTION_RIGHT, current_output, 1, 0);
		break;
	case VIEW_EDGE_UP:
		new_output = wlr_output_layout_adjacent_output(
			layout, WLR_DIRECTION_UP, current_output, 0, 1);
		break;
	case VIEW_EDGE_DOWN:
		new_output = wlr_output_layout_adjacent_output(
			layout, WLR_DIRECTION_DOWN, current_output, 0, 1);
		break;
	default:
		break;
	}

	/* When "adjacent" output is the same as the original, there is no adjacent */
	if (!new_output || new_output == current_output) {
		return NULL;
	}

	output = output_from_wlr_output(view->server, new_output);
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "invalid output in layout");
		return NULL;
	}

	return output;
}

void
view_move_to_edge(struct view *view, enum view_edge direction, bool snap_to_windows)
{
	assert(view);
	if (!output_is_usable(view->output)) {
		wlr_log(WLR_ERROR, "view has no output, not moving to edge");
		return;
	}

	int dx = 0, dy = 0;
	if (snap_to_windows) {
		snap_vector_to_next_edge(view, direction, &dx, &dy);
	} else {
		struct border distance = snap_get_max_distance(view);
		switch (direction) {
		case VIEW_EDGE_LEFT:
			dx = distance.left;
			break;
		case VIEW_EDGE_UP:
			dy = distance.top;
			break;
		case VIEW_EDGE_RIGHT:
			dx = distance.right;
			break;
		case VIEW_EDGE_DOWN:
			dy = distance.bottom;
			break;
		default:
			return;
		}
	}

	if (dx != 0 || dy != 0) {
		/* Move the window if a change was discovered */
		view_move(view, view->pending.x + dx, view->pending.y + dy);
		return;
	}

	/* If the view is maximized, do not attempt to jump displays */
	if (view->maximized != VIEW_AXIS_NONE) {
		return;
	}

	/* Otherwise, move to edge of next adjacent display, if possible */
	struct output *output = view_get_adjacent_output(view, direction);
	if (!output) {
		return;
	}

	/* When jumping to next output, attach to edge nearest the motion */
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	struct border margin = ssd_get_margin(view->ssd);

	/* Bounds of the possible placement zone in this output */
	int left = usable.x + rc.gap + margin.left;
	int right = usable.x + usable.width - rc.gap - margin.right;
	int top = usable.y + rc.gap + margin.top;
	int bottom = usable.y + usable.height - rc.gap - margin.bottom;

	/* Default target position on new output is current target position */
	int destination_x = view->pending.x;
	int destination_y = view->pending.y;

	/* Compute the new position in the direction of motion */
	direction = view_edge_invert(direction);
	switch (direction) {
	case VIEW_EDGE_LEFT:
		destination_x = left;
		break;
	case VIEW_EDGE_RIGHT:
		destination_x = right - view->pending.width;
		break;
	case VIEW_EDGE_UP:
		destination_y = top;
		break;
	case VIEW_EDGE_DOWN:
		destination_y = bottom - view->pending.height;
		break;
	default:
		return;
	}

	/* If more than half the view is right of usable region, align to right */
	int midpoint = destination_x + view->pending.width / 2;

	if (destination_x >= left && midpoint > usable.x + usable.width) {
		destination_x = right - view->pending.width;
	}

	/* Never allow the window to start left of the usable edge */
	destination_x = MAX(destination_x, left);

	/* If more than half the view is below usable region, align to bottom */
	midpoint = destination_y + view->pending.height / 2;
	if (destination_y >= top && midpoint > usable.y + usable.height) {
		destination_y = bottom - view->pending.height;
	}

	/* Never allow the window to start above the usable edge */
	destination_y = MAX(destination_y, top);

	view_set_untiled(view);
	view_set_output(view, output);
	view_move(view, destination_x, destination_y);
}

void
view_grow_to_edge(struct view *view, enum view_edge direction)
{
	assert(view);
	/* TODO: allow grow to edge if maximized along the other axis */
	if (view->fullscreen || view->maximized != VIEW_AXIS_NONE) {
		return;
	}
	if (!output_is_usable(view->output)) {
		wlr_log(WLR_ERROR, "view has no output, not growing view");
		return;
	}

	struct wlr_box geo = view->pending;
	snap_grow_to_next_edge(view, direction, &geo);
	view_move_resize(view, geo);
}

void
view_shrink_to_edge(struct view *view, enum view_edge direction)
{
	assert(view);
	/* TODO: allow shrink to edge if maximized along the other axis */
	if (view->fullscreen || view->maximized != VIEW_AXIS_NONE) {
		return;
	}
	if (!output_is_usable(view->output)) {
		wlr_log(WLR_ERROR, "view has no output, not shrinking view");
		return;
	}

	struct wlr_box geo = view->pending;
	snap_shrink_to_next_edge(view, direction, &geo);
	view_move_resize(view, geo);
}

enum view_axis
view_axis_parse(const char *direction)
{
	if (!direction) {
		return VIEW_AXIS_NONE;
	}
	if (!strcasecmp(direction, "horizontal")) {
		return VIEW_AXIS_HORIZONTAL;
	} else if (!strcasecmp(direction, "vertical")) {
		return VIEW_AXIS_VERTICAL;
	} else if (!strcasecmp(direction, "both")) {
		return VIEW_AXIS_BOTH;
	} else {
		return VIEW_AXIS_NONE;
	}
}

enum view_edge
view_edge_parse(const char *direction)
{
	if (!direction) {
		return VIEW_EDGE_INVALID;
	}
	if (!strcasecmp(direction, "left")) {
		return VIEW_EDGE_LEFT;
	} else if (!strcasecmp(direction, "up")) {
		return VIEW_EDGE_UP;
	} else if (!strcasecmp(direction, "right")) {
		return VIEW_EDGE_RIGHT;
	} else if (!strcasecmp(direction, "down")) {
		return VIEW_EDGE_DOWN;
	} else if (!strcasecmp(direction, "center")) {
		return VIEW_EDGE_CENTER;
	} else {
		return VIEW_EDGE_INVALID;
	}
}

void
view_snap_to_edge(struct view *view, enum view_edge edge,
			bool across_outputs, bool store_natural_geometry)
{
	assert(view);
	if (view->fullscreen) {
		return;
	}
	struct output *output = view->output;
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "view has no output, not snapping to edge");
		return;
	}

	if (across_outputs && view->tiled == edge && view->maximized == VIEW_AXIS_NONE) {
		/* We are already tiled for this edge; try to switch outputs */
		output = view_get_adjacent_output(view, edge);

		if (!output) {
			/*
			 * No more output to move to
			 *
			 * We re-apply the tiled geometry without changing any
			 * state because the window might have been moved away
			 * (and thus got untiled) and then snapped back to the
			 * original edge.
			 */
			view_apply_tiled_geometry(view);
			return;
		}

		/* When switching outputs, jump to the opposite edge */
		edge = view_edge_invert(edge);
	}

	if (view->maximized != VIEW_AXIS_NONE) {
		/* Unmaximize + keep using existing natural_geometry */
		view_maximize(view, VIEW_AXIS_NONE,
			/*store_natural_geometry*/ false);
	} else if (store_natural_geometry) {
		/* store current geometry as new natural_geometry */
		view_store_natural_geometry(view);
		view_invalidate_last_layout_geometry(view);
	}
	view_set_untiled(view);
	view_set_output(view, output);
	view->tiled = edge;
	view_apply_tiled_geometry(view);
}

void
view_snap_to_region(struct view *view, struct region *region,
		bool store_natural_geometry)
{
	assert(view);
	assert(region);
	if (view->fullscreen) {
		return;
	}
	/* view_apply_region_geometry() needs a usable output */
	if (!output_is_usable(view->output)) {
		wlr_log(WLR_ERROR, "view has no output, not snapping to region");
		return;
	}

	if (view->maximized != VIEW_AXIS_NONE) {
		/* Unmaximize + keep using existing natural_geometry */
		view_maximize(view, VIEW_AXIS_NONE,
			/*store_natural_geometry*/ false);
	} else if (store_natural_geometry) {
		/* store current geometry as new natural_geometry */
		view_store_natural_geometry(view);
		view_invalidate_last_layout_geometry(view);
	}
	view_set_untiled(view);
	view->tiled_region = region;
	view_apply_region_geometry(view);
}

static void
for_each_subview(struct view *view, void (*action)(struct view *))
{
	struct wl_array subviews;
	struct view **subview;

	wl_array_init(&subviews);
	view_append_children(view, &subviews);
	wl_array_for_each(subview, &subviews) {
		action(*subview);
	}
	wl_array_release(&subviews);
}

static void
move_to_front(struct view *view)
{
	if (view->impl->move_to_front) {
		view->impl->move_to_front(view);
	}
	view->server->last_raised_view = view;
}

static void
move_to_back(struct view *view)
{
	if (view->impl->move_to_back) {
		view->impl->move_to_back(view);
	}
	if (view == view->server->last_raised_view) {
		view->server->last_raised_view = NULL;
	}
}

/*
 * In the view_move_to_{front,back} functions, a modal dialog is always
 * shown above its parent window, and the two always move together, so
 * other windows cannot come between them.
 * This is consistent with GTK3/Qt5 applications on mutter and openbox.
 */
void
view_move_to_front(struct view *view)
{
	assert(view);
	/*
	 * This function is called often, generally on every mouse
	 * button press (more often for focus-follows-mouse). Avoid
	 * unnecessarily raising the same view over and over, or
	 * attempting to raise a root view above its own sub-view.
	 */
	struct view *last = view->server->last_raised_view;
	if (view == last || (last && view == view_get_root(last))) {
		return;
	}

	struct view *root = view_get_root(view);
	assert(root);

	move_to_front(root);
	for_each_subview(root, move_to_front);
	/* make sure view is in front of other sub-views */
	if (view != root) {
		move_to_front(view);
	}

	cursor_update_focus(view->server);
}

void
view_move_to_back(struct view *view)
{
	assert(view);
	struct view *root = view_get_root(view);
	assert(root);

	for_each_subview(root, move_to_back);
	move_to_back(root);

	cursor_update_focus(view->server);
}

struct view *
view_get_root(struct view *view)
{
	assert(view);
	if (view->impl->get_root) {
		return view->impl->get_root(view);
	}
	return view;
}

void
view_append_children(struct view *view, struct wl_array *children)
{
	assert(view);
	if (view->impl->append_children) {
		view->impl->append_children(view, children);
	}
}

bool
view_is_related(struct view *view, struct wlr_surface *surface)
{
	assert(view);
	assert(surface);
	if (view->impl->is_related) {
		return view->impl->is_related(view, surface);
	}
	return false;
}

bool
view_has_strut_partial(struct view *view)
{
	assert(view);
	return view->impl->has_strut_partial &&
		view->impl->has_strut_partial(view);
}

const char *
view_get_string_prop(struct view *view, const char *prop)
{
	assert(view);
	assert(prop);
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, prop);
	}
	return "";
}

void
view_update_title(struct view *view)
{
	assert(view);
	const char *title = view_get_string_prop(view, "title");
	if (!view->toplevel.handle || !title) {
		return;
	}
	ssd_update_title(view->ssd);
	wlr_foreign_toplevel_handle_v1_set_title(view->toplevel.handle, title);
}

void
view_update_app_id(struct view *view)
{
	assert(view);
	const char *app_id = view_get_string_prop(view, "app_id");
	if (!view->toplevel.handle || !app_id) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_app_id(
		view->toplevel.handle, app_id);
}

void
view_reload_ssd(struct view *view)
{
	assert(view);
	if (view->ssd_enabled && !view->fullscreen) {
		undecorate(view);
		decorate(view);
	}
}

void
view_toggle_keybinds(struct view *view)
{
	assert(view);
	view->inhibits_keybinds = !view->inhibits_keybinds;
	if (view->inhibits_keybinds) {
		view->server->seat.nr_inhibited_keybind_views++;
	} else {
		view->server->seat.nr_inhibited_keybind_views--;
	}

	if (view->ssd_enabled) {
		ssd_enable_keybind_inhibit_indicator(view->ssd,
			view->inhibits_keybinds);
	}
}

void
mappable_connect(struct mappable *mappable, struct wlr_surface *surface,
		wl_notify_func_t notify_map, wl_notify_func_t notify_unmap)
{
	assert(mappable);
	assert(!mappable->connected);
	mappable->map.notify = notify_map;
	wl_signal_add(&surface->events.map, &mappable->map);
	mappable->unmap.notify = notify_unmap;
	wl_signal_add(&surface->events.unmap, &mappable->unmap);
	mappable->connected = true;
}

void
mappable_disconnect(struct mappable *mappable)
{
	assert(mappable);
	assert(mappable->connected);
	wl_list_remove(&mappable->map.link);
	wl_list_remove(&mappable->unmap.link);
	mappable->connected = false;
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, mappable.map);
	view->impl->map(view);
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, mappable.unmap);
	view->impl->unmap(view, /* client_request */ true);
}

/*
 * TODO: after the release of wlroots 0.17, consider incorporating this
 * function into a more general view_set_surface() function, which could
 * connect other surface event handlers (like commit) as well.
 */
void
view_connect_map(struct view *view, struct wlr_surface *surface)
{
	assert(view);
	mappable_connect(&view->mappable, surface, handle_map, handle_unmap);
}

void
view_destroy(struct view *view)
{
	assert(view);
	struct server *server = view->server;
	bool need_cursor_update = false;

	if (view->mappable.connected) {
		mappable_disconnect(&view->mappable);
	}

	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_minimize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	wl_list_remove(&view->set_title.link);
	wl_list_remove(&view->destroy.link);

	if (view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_destroy(view->toplevel.handle);
	}

	if (server->grabbed_view == view) {
		/* Application got killed while moving around */
		server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
		server->grabbed_view = NULL;
		need_cursor_update = true;
		regions_hide_overlay(&server->seat);
	}

	if (server->active_view == view) {
		server->active_view = NULL;
		need_cursor_update = true;
	}

	if (server->last_raised_view == view) {
		server->last_raised_view = NULL;
	}

	if (server->seat.pressed.view == view) {
		seat_reset_pressed(&server->seat);
	}

	if (view->tiled_region_evacuate) {
		zfree(view->tiled_region_evacuate);
	}

	if (view->inhibits_keybinds) {
		view->inhibits_keybinds = false;
		server->seat.nr_inhibited_keybind_views--;
	}

	osd_on_view_destroy(view);
	undecorate(view);

	if (view->scene_tree) {
		wlr_scene_node_destroy(&view->scene_tree->node);
		view->scene_tree = NULL;
	}

	/*
	 * The layer-shell top-layer is disabled when an application is running
	 * in fullscreen mode, so if that's the case, we may have to re-enable
	 * it here.
	 */
	if (view->fullscreen && view->output) {
		view->fullscreen = false;
		desktop_update_top_layer_visiblity(server);
		if (rc.adaptive_sync == LAB_ADAPTIVE_SYNC_FULLSCREEN) {
			set_adaptive_sync_fullscreen(view);
		}
	}

	/* If we spawned a window menu, close it */
	if (server->menu_current
			&& server->menu_current->triggered_by_view == view) {
		menu_close_root(server);
	}

	/* Remove view from server->views */
	wl_list_remove(&view->link);
	free(view);

	if (need_cursor_update) {
		cursor_update_focus(server);
	}
}
