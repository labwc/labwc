// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_security_context_v1.h>
#include "common/box.h"
#include "common/macros.h"
#include "common/match.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/scene-helpers.h"
#include "foreign-toplevel.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "menu/menu.h"
#include "osd.h"
#include "output-state.h"
#include "placement.h"
#include "regions.h"
#include "resize-indicator.h"
#include "snap-constraints.h"
#include "snap.h"
#include "ssd.h"
#include "view.h"
#include "window-rules.h"
#include "wlr/util/log.h"
#include "workspaces.h"
#include "xwayland.h"

#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif

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

static const struct wlr_security_context_v1_state *
security_context_from_view(struct view *view)
{
	if (view && view->surface && view->surface->resource) {
		struct wl_client *client = wl_resource_get_client(view->surface->resource);
		return wlr_security_context_manager_v1_lookup_client(
			view->server->security_context_manager_v1, client);
	}
	return NULL;
}

struct view_query *
view_query_create(void)
{
	struct view_query *query = znew(*query);
	query->window_type = -1;
	query->maximized = VIEW_AXIS_INVALID;
	return query;
}

void
view_query_free(struct view_query *query)
{
	wl_list_remove(&query->link);
	zfree(query->identifier);
	zfree(query->title);
	zfree(query->sandbox_engine);
	zfree(query->sandbox_app_id);
	zfree(query->tiled_region);
	zfree(query->desktop);
	zfree(query->monitor);
	zfree(query);
}

static bool
query_tristate_match(enum three_state desired, bool actual)
{
	switch (desired) {
	case LAB_STATE_ENABLED:
		return actual;
	case LAB_STATE_DISABLED:
		return !actual;
	default:
		return true;
	}
}

static bool
query_str_match(const char *condition, const char *value)
{
	if (!condition) {
		return true;
	}
	return value && match_glob(condition, value);
}

bool
view_matches_query(struct view *view, struct view_query *query)
{
	if (!query_str_match(query->identifier, view_get_string_prop(view, "app_id"))) {
		return false;
	}

	if (!query_str_match(query->title, view_get_string_prop(view, "title"))) {
		return false;
	}

	if (query->window_type >= 0 && !view_contains_window_type(view, query->window_type)) {
		return false;
	}

	if (query->sandbox_engine || query->sandbox_app_id) {
		const struct wlr_security_context_v1_state *ctx =
			security_context_from_view(view);

		if (!ctx) {
			return false;
		}

		if (!query_str_match(query->sandbox_engine, ctx->sandbox_engine)) {
			return false;
		}

		if (!query_str_match(query->sandbox_app_id, ctx->app_id)) {
			return false;
		}
	}

	if (!query_tristate_match(query->shaded, view->shaded)) {
		return false;
	}

	if (query->maximized != VIEW_AXIS_INVALID && view->maximized != query->maximized) {
		return false;
	}

	if (!query_tristate_match(query->iconified, view->minimized)) {
		return false;
	}

	if (!query_tristate_match(query->focused, view->server->active_view == view)) {
		return false;
	}

	if (!query_tristate_match(query->omnipresent, view->visible_on_all_workspaces)) {
		return false;
	}

	if (query->tiled != VIEW_EDGE_INVALID && query->tiled != view->tiled) {
		return false;
	}

	const char *tiled_region =
		view->tiled_region ? view->tiled_region->name : NULL;
	if (!query_str_match(query->tiled_region, tiled_region)) {
		return false;
	}

	if (query->desktop) {
		const char *view_workspace = view->workspace->name;
		struct workspace *current = view->server->workspaces.current;

		if (!strcasecmp(query->desktop, "other")) {
			/* "other" means the view is NOT on the current desktop */
			if (!strcasecmp(view_workspace, current->name)) {
				return false;
			}
		} else {
			// TODO: perhaps wrap "left" and "right" workspaces
			struct workspace *target =
				workspaces_find(current, query->desktop, /* wrap */ false);
			if (!target || strcasecmp(view_workspace, target->name)) {
				return false;
			}
		}
	}

	enum ssd_mode decor = view_get_ssd_mode(view);
	if (query->decoration != LAB_SSD_MODE_INVALID && query->decoration != decor) {
		return false;
	}

	if (query->monitor) {
		struct output *target = output_from_name(view->server, query->monitor);
		if (target != view->output) {
			return false;
		}
	}

	return true;
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
		if (view->scene_tree->node.parent != server->workspaces.current->tree
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
	if (criteria & LAB_VIEW_CRITERIA_ROOT_TOPLEVEL) {
		if (view != view_get_root(view)) {
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

struct view *
view_next_no_head_stop(struct wl_list *head, struct view *from,
		enum lab_view_criteria criteria)
{
	assert(head);

	struct wl_list *elm = from ? &from->link : head;

	struct wl_list *end = elm;
	for (elm = elm->next; elm != end; elm = elm->next) {
		if (elm == head) {
			continue;
		}
		struct view *view = wl_container_of(elm, view, link);
		if (matches_criteria(view, criteria)) {
			return view;
		}
	}
	return from;
}

struct view *
view_prev_no_head_stop(struct wl_list *head, struct view *from,
		enum lab_view_criteria criteria)
{
	assert(head);

	struct wl_list *elm = from ? &from->link : head;

	struct wl_list *end = elm;
	for (elm = elm->prev; elm != end; elm = elm->prev) {
		if (elm == head) {
			continue;
		}
		struct view *view = wl_container_of(elm, view, link);
		if (matches_criteria(view, criteria)) {
			return view;
		}
	}
	return from;
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
view_contains_window_type(struct view *view, enum window_type window_type)
{
	assert(view);
	if (view->impl->contains_window_type) {
		return view->impl->contains_window_type(view, window_type);
	}
	return false;
}

bool
view_is_focusable(struct view *view)
{
	assert(view);
	if (!view->surface) {
		return false;
	}
	if (view_wants_focus(view) != VIEW_WANTS_FOCUS_ALWAYS) {
		return false;
	}
	return (view->mapped || view->minimized);
}

/**
 * All view_apply_xxx_geometry() functions must *not* modify
 * any state besides repositioning or resizing the view.
 *
 * They may be called repeatably during output layout changes.
 */

enum view_edge
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

	if (!geometry) {
		geometry = &view->current;
	}

	struct output *output =
		output_nearest_to(view->server,
			geometry->x + geometry->width / 2,
			geometry->y + geometry->height / 2);

	if (output && output != view->output) {
		view->output = output;
		/* Show fullscreen views above top-layer */
		if (view->fullscreen) {
			desktop_update_top_layer_visiblity(view->server);
		}
		return true;
	}

	return false;
}

static void
set_adaptive_sync_fullscreen(struct view *view)
{
	if (!output_is_usable(view->output)) {
		return;
	}
	if (rc.adaptive_sync != LAB_ADAPTIVE_SYNC_FULLSCREEN) {
		return;
	}
	/* Enable adaptive sync if view is fullscreen */
	output_enable_adaptive_sync(view->output, view->fullscreen);
	output_state_commit(view->output);
}

void
view_set_activated(struct view *view, bool activated)
{
	assert(view);
	ssd_set_active(view->ssd, activated);
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}

	wl_signal_emit_mutable(&view->events.activated, &activated);

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
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "invalid output set for view");
		return;
	}
	view->output = output;
	/* Show fullscreen views above top-layer */
	if (view->fullscreen) {
		desktop_update_top_layer_visiblity(view->server);
	}
}

void
view_close(struct view *view)
{
	assert(view);
	if (view->impl->close) {
		view->impl->close(view);
	}
}

static void
view_update_outputs(struct view *view)
{
	struct output *output;
	struct wlr_output_layout *layout = view->server->output_layout;

	uint64_t new_outputs = 0;
	wl_list_for_each(output, &view->server->outputs, link) {
		if (output_is_usable(output) && wlr_output_layout_intersects(
				layout, output->wlr_output, &view->current)) {
			new_outputs |= (1ull << output->scene_output->index);
		}
	}

	if (new_outputs != view->outputs) {
		view->outputs = new_outputs;
		wl_signal_emit_mutable(&view->events.new_outputs, NULL);
		desktop_update_top_layer_visiblity(view->server);
	}
}

bool
view_on_output(struct view *view, struct output *output)
{
	assert(view);
	assert(output);
	return output->scene_output
			&& (view->outputs & (1ull << output->scene_output->index));
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
	view_update_outputs(view);
	ssd_update_geometry(view->ssd);
	cursor_update_focus(view->server);
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
	view_set_shade(view, false);
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

	/* Limit usable region to account for gap */
	usable.x += rc.gap;
	usable.y += rc.gap;
	usable.width -= 2 * rc.gap;
	usable.height -= 2 * rc.gap;

	if (x + geo.width > usable.x + usable.width) {
		x = usable.x + usable.width - geo.width;
	}
	x = MAX(x, usable.x) + margin.left;

	if (y + geo.height > usable.y + usable.height) {
		y = usable.y + usable.height - geo.height;
	}
	y = MAX(y, usable.y) + margin.top;

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
	int min_width = view_get_min_width();

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
		hints.min_width = min_width;
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

	if (view->impl->minimize) {
		view->impl->minimize(view, minimized);
	}

	view->minimized = minimized;
	wl_signal_emit_mutable(&view->events.minimized, NULL);

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

	/* Enable top-layer when full-screen views are minimized */
	if (view->fullscreen && view->output) {
		desktop_update_top_layer_visiblity(view->server);
	}
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

	/* Fit the view within the usable area */
	if (*x < usable.x) {
		*x = usable.x;
	} else if (*x + width > usable.x + usable.width) {
		*x = usable.x + usable.width - width;
	}
	if (*y < usable.y) {
		*y = usable.y;
	} else if (*y + height > usable.y + usable.height) {
		*y = usable.y + usable.height - height;
	}

	*x += margin.left;
	*y += margin.top;

	return true;
}

static bool
adjust_floating_geometry(struct view *view, struct wlr_box *geometry,
		bool midpoint_visibility)
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
	bool onscreen = false;
	if (wlr_output_layout_intersects(view->server->output_layout,
			view->output->wlr_output, geometry)) {
		/* Always make sure the titlebar starts within the usable area */
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

		if (!midpoint_visibility) {
			/*
			 * If midpoint visibility is not required, the view is
			 * on screen if at least one pixel is visible.
			 */
			onscreen = true;
		} else {
			/* Otherwise, make sure the midpoint is on screen */
			int mx = geometry->x + geometry->width / 2;
			int my = geometry->y + geometry->height / 2;

			onscreen = mx <= usable.x + usable.width &&
				my <= usable.y + usable.height;
		}
	}

	if (onscreen) {
		return adjusted;
	}

	/* Reposition offscreen automatically if configured to do so */
	if (rc.placement_policy == LAB_PLACE_AUTOMATIC) {
		if (placement_find_best(view, geometry)) {
			return true;
		}
	}

	/* If automatic placement failed or was not enabled, just center */
	return view_compute_centered_position(view, NULL,
		geometry->width, geometry->height,
		&geometry->x, &geometry->y);
}

void
view_set_fallback_natural_geometry(struct view *view)
{
	view->natural_geometry.width = VIEW_FALLBACK_WIDTH;
	view->natural_geometry.height = VIEW_FALLBACK_HEIGHT;
	view_compute_centered_position(view, NULL,
		view->natural_geometry.width,
		view->natural_geometry.height,
		&view->natural_geometry.x,
		&view->natural_geometry.y);
}

void
view_store_natural_geometry(struct view *view)
{
	assert(view);
	if (!view_is_floating(view)) {
		/* Do not overwrite the stored geometry with special cases */
		return;
	}

	/*
	 * Note that for xdg-shell views that start fullscreen or maximized,
	 * we end up storing a natural geometry of 0x0. This is intentional.
	 * When leaving fullscreen or unmaximizing, we pass 0x0 to the
	 * xdg-toplevel configure event, which means the application should
	 * choose its own size.
	 */
	view->natural_geometry = view->pending;
}

int
view_effective_height(struct view *view, bool use_pending)
{
	assert(view);

	if (view->shaded) {
		return 0;
	}

	return use_pending ? view->pending.height : view->current.height;
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

/*
 * Algorithm based on KWin's implementation:
 * https://github.com/KDE/kwin/blob/df9f8f8346b5b7645578e37365dabb1a7b02ca5a/src/placement.cpp#L589
 */
static void
view_cascade(struct view *view)
{
	/* "cascade" policy places a new view at center by default */
	struct wlr_box center = view->pending;
	view_compute_centered_position(view, NULL,
		center.width, center.height, &center.x, &center.y);
	struct border margin = ssd_get_margin(view->ssd);
	center.x -= margin.left;
	center.y -= margin.top;
	center.width += margin.left + margin.right;
	center.height += margin.top + margin.bottom;

	/* Candidate geometry to which the view is moved */
	struct wlr_box candidate = center;

	struct wlr_box usable = output_usable_area_in_layout_coords(view->output);

	/* TODO: move this logic to rcxml.c */
	int offset_x = rc.placement_cascade_offset_x;
	int offset_y = rc.placement_cascade_offset_y;
	struct theme *theme = view->server->theme;
	int default_offset = theme->titlebar_height + theme->border_width + 5;
	if (offset_x <= 0) {
		offset_x = default_offset;
	}
	if (offset_y <= 0) {
		offset_y = default_offset;
	}

	/*
	 * Keep updating the candidate until it doesn't cover any existing views
	 * or doesn't fit within the usable area.
	 */
	bool candidate_updated = true;
	while (candidate_updated) {
		candidate_updated = false;
		struct wlr_box covered = {0};

		/* Iterate over views from top to bottom */
		struct view *other_view;
		for_each_view(other_view, &view->server->views,
				LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
			struct wlr_box other = ssd_max_extents(other_view);
			if (other_view == view
					|| view->minimized
					|| !box_intersects(&candidate, &other)) {
				continue;
			}
			/*
			 * If the candidate covers an existing view whose
			 * top-left corner is not covered by other views,
			 * shift the candidate to bottom-right.
			 */
			if (box_contains(&candidate, &other)
					&& !wlr_box_contains_point(
						&covered, other.x, other.y)) {
				candidate.x = other.x + offset_x;
				candidate.y = other.y + offset_y;
				if (!box_contains(&usable, &candidate)) {
					/*
					 * If the candidate doesn't fit within
					 * the usable area, fall back to center
					 * and finish updating the candidate.
					 */
					candidate = center;
					break;
				} else {
					/* Repeat with the new candidate */
					candidate_updated = true;
					break;
				}
			}
			/*
			 * We use just a bounding box to represent the covered
			 * area, which would be fine for our use-case.
			 */
			box_union(&covered, &covered, &other);
		}
	}

	view_move(view, candidate.x + margin.left, candidate.y + margin.top);
}

void
view_place_by_policy(struct view *view, bool allow_cursor,
		enum view_placement_policy policy)
{
	if (allow_cursor && policy == LAB_PLACE_CURSOR) {
		view_move_to_cursor(view);
		return;
	} else if (policy == LAB_PLACE_AUTOMATIC) {
		struct wlr_box geometry = view->pending;
		if (placement_find_best(view, &geometry)) {
			view_move(view, geometry.x, geometry.y);
			return;
		}
	} else if (policy == LAB_PLACE_CASCADE) {
		view_cascade(view);
		return;
	}

	view_center(view, NULL);
}

void
view_constrain_size_to_that_of_usable_area(struct view *view)
{
	if (!view || !view->output || view->fullscreen) {
		return;
	}

	struct wlr_box usable_area =
			output_usable_area_in_layout_coords(view->output);
	struct border margin = ssd_get_margin(view->ssd);

	int available_width = usable_area.width - margin.left - margin.right;
	int available_height = usable_area.height - margin.top - margin.bottom;

	if (available_width <= 0 || available_height <= 0) {
		return;
	}

	if (available_height >= view->pending.height &&
			available_width >= view->pending.width) {
		return;
	}

	int width = MIN(view->pending.width, available_width);
	int height = MIN(view->pending.height, available_height);

	int right_edge = usable_area.x + usable_area.width;
	int bottom_edge = usable_area.y + usable_area.height;

	int x =
		MAX(usable_area.x + margin.left,
			MIN(view->pending.x, right_edge - width - margin.right));

	int y =
		MAX(usable_area.y + margin.top,
			MIN(view->pending.y, bottom_edge - height - margin.bottom));

	struct wlr_box box = {
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};
	view_move_resize(view, box);
}

void
view_apply_natural_geometry(struct view *view)
{
	assert(view);
	assert(view_is_floating(view));

	struct wlr_box geometry = view->natural_geometry;
	/* Only adjust natural geometry if known (not 0x0) */
	if (!wlr_box_empty(&geometry)) {
		adjust_floating_geometry(view, &geometry,
			/* midpoint_visibility */ false);
	}
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
	if (view->maximized != VIEW_AXIS_BOTH
			&& !box_intersects(&box, &natural)) {
		view_compute_centered_position(view, NULL,
			natural.width, natural.height,
			&natural.x, &natural.y);
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

	view->maximized = maximized;
	wl_signal_emit_mutable(&view->events.maximized, NULL);

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
view_is_tiled_and_notify_tiled(struct view *view)
{
	switch (rc.snap_tiling_events_mode) {
	case LAB_TILING_EVENTS_NEVER:
		return false;
	case LAB_TILING_EVENTS_REGION:
		return view->tiled_region || view->tiled_region_evacuate;
	case LAB_TILING_EVENTS_EDGE:
		return view->tiled;
	case LAB_TILING_EVENTS_ALWAYS:
		return view_is_tiled(view);
	}

	return false;
}

bool
view_is_floating(struct view *view)
{
	assert(view);
	return !(view->fullscreen || (view->maximized != VIEW_AXIS_NONE)
		|| view_is_tiled(view));
}

static void
view_notify_tiled(struct view *view)
{
	assert(view);
	if (view->impl->notify_tiled) {
		view->impl->notify_tiled(view);
	}
}

/* Reset tiled state of view without changing geometry */
void
view_set_untiled(struct view *view)
{
	assert(view);
	view->tiled = VIEW_EDGE_INVALID;
	view->tiled_region = NULL;
	zfree(view->tiled_region_evacuate);
	view_notify_tiled(view);
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

	view_set_shade(view, false);

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

	/*
	 * When natural geometry is unknown (0x0) for an xdg-shell view,
	 * we normally send a configure event of 0x0 to get the client's
	 * preferred size, but this doesn't work if unmaximizing only
	 * one axis. So in that corner case, set a fallback geometry.
	 */
	if ((axis == VIEW_AXIS_HORIZONTAL || axis == VIEW_AXIS_VERTICAL)
			&& wlr_box_empty(&view->natural_geometry)) {
		view_set_fallback_natural_geometry(view);
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

bool
view_wants_decorations(struct view *view)
{
	/* Window-rules take priority if they exist for this view */
	switch (window_rules_get_property(view, "serverDecoration")) {
	case LAB_PROP_TRUE:
		return true;
	case LAB_PROP_FALSE:
		return false;
	default:
		break;
	}

	/*
	 * view->ssd_preference may be set by the decoration implementation
	 * e.g. src/decorations/xdg-deco.c or src/decorations/kde-deco.c.
	 */
	switch (view->ssd_preference) {
	case LAB_SSD_PREF_SERVER:
		return true;
	case LAB_SSD_PREF_CLIENT:
		return false;
	default:
		/*
		 * We don't know anything about the client preference
		 * so fall back to core.decoration settings in rc.xml
		 */
		return rc.xdg_shell_server_side_deco;
	}
}

void
view_set_decorations(struct view *view, enum ssd_mode mode, bool force_ssd)
{
	assert(view);

	if (force_ssd || view_wants_decorations(view)
			|| mode < view_get_ssd_mode(view)) {
		view_set_ssd_mode(view, mode);
	}
}

void
view_toggle_decorations(struct view *view)
{
	assert(view);

	enum ssd_mode mode = view_get_ssd_mode(view);
	if (rc.ssd_keep_border && mode == LAB_SSD_MODE_FULL) {
		view_set_ssd_mode(view, LAB_SSD_MODE_BORDER);
	} else if (mode != LAB_SSD_MODE_NONE) {
		view_set_ssd_mode(view, LAB_SSD_MODE_NONE);
	} else {
		view_set_ssd_mode(view, LAB_SSD_MODE_FULL);
	}
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
		view->workspace = view->server->workspaces.current;
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
		view->workspace = view->server->workspaces.current;
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
	ssd_update_geometry(view->ssd);
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

enum ssd_mode
view_get_ssd_mode(struct view *view)
{
	assert(view);

	if (!view->ssd_enabled) {
		return LAB_SSD_MODE_NONE;
	} else if (view->ssd_titlebar_hidden) {
		return LAB_SSD_MODE_BORDER;
	} else {
		return LAB_SSD_MODE_FULL;
	}
}

void
view_set_ssd_mode(struct view *view, enum ssd_mode mode)
{
	assert(view);

	if (view->shaded || view->fullscreen
			|| mode == view_get_ssd_mode(view)) {
		return;
	}

	/*
	 * Set these first since they are referenced
	 * within the call tree of ssd_create() and ssd_thickness()
	 */
	view->ssd_enabled = mode != LAB_SSD_MODE_NONE;
	view->ssd_titlebar_hidden = mode != LAB_SSD_MODE_FULL;

	if (view->ssd_enabled) {
		decorate(view);
		ssd_set_titlebar(view->ssd, !view->ssd_titlebar_hidden);
	} else {
		undecorate(view);
	}

	if (!view_is_floating(view)) {
		view_apply_special_geometry(view);
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
	/* When going fullscreen, unshade the window */
	if (fullscreen) {
		view_set_shade(view, false);
	}

	/* Hide decorations when going fullscreen */
	if (fullscreen && view->ssd_enabled) {
		undecorate(view);
	}

	if (view->impl->set_fullscreen) {
		view->impl->set_fullscreen(view, fullscreen);
	}

	view->fullscreen = fullscreen;
	wl_signal_emit_mutable(&view->events.fullscreened, NULL);

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
	adjust_floating_geometry(view, &view->natural_geometry,
		/* midpoint_visibility */ true);
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

	bool is_floating = view_is_floating(view);
	bool use_natural = false;

	if (!output_is_usable(view->output)) {
		/* A view losing an output should have a last-layout geometry */
		update_last_layout_geometry(view);
	}

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
		 * Move the window to its natural location, because
		 * we are trying to restore a prior layout.
		 */
		view_apply_natural_geometry(view);
	} else {
		/* Otherwise, just ensure the view is on screen. */
		struct wlr_box geometry = view->pending;
		if (adjust_floating_geometry(view, &geometry,
					/* midpoint_visibility */ true)) {
			view_move_resize(view, geometry);
		}
	}

	view_update_outputs(view);
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
	view->output = NULL;
}

static int
shift_view_to_usable_1d(int size,
		int cur_pos, int cur_lo, int cur_extent,
		int next_pos, int next_lo, int next_extent,
		int margin_lo, int margin_hi)
{
	int cur_min = cur_lo + rc.gap + margin_lo;
	int cur_max = cur_lo + cur_extent - rc.gap - margin_hi;

	int next_min = next_lo + rc.gap + margin_lo;
	int next_max = next_lo + next_extent - rc.gap - margin_hi;

	/*
	 * If the view is fully within the usable area of its original display,
	 * ensure that it is also fully within the usable area of the target.
	 */
	if (cur_pos >= cur_min && cur_pos + size <= cur_max) {
		if (next_pos >= next_min && next_pos + size > next_max) {
			next_pos = next_max - size;
		}

		return MAX(next_pos, next_min);
	}

	/*
	 * If the view was not fully within the usable area of its original
	 * display, kick it onscreen if its midpoint will be off the target.
	 */
	int midpoint = next_pos + size / 2;
	if (next_pos >= next_min && midpoint > next_lo + next_extent) {
		next_pos = next_max - size;
	}

	return MAX(next_pos, next_min);
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
	snap_move_to_edge(view, direction, snap_to_windows, &dx, &dy);

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
	struct output *output =
		output_get_adjacent(view->output, direction, /* wrap */ false);
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
		destination_y = bottom
			- view_effective_height(view, /* use_pending */ true);
		break;
	default:
		return;
	}

	struct wlr_box original_usable =
		output_usable_area_in_layout_coords(view->output);

	/* Make sure the window is appropriately in view along the x direction */
	destination_x = shift_view_to_usable_1d(view->pending.width,
		view->pending.x, original_usable.x, original_usable.width,
		destination_x, usable.x, usable.width, margin.left, margin.right);

	/* Make sure the window is appropriately in view along the y direction */
	int eff_height = view_effective_height(view, /* use_pending */ true);
	destination_y = shift_view_to_usable_1d(eff_height,
		view->pending.y, original_usable.y, original_usable.height,
		destination_y, usable.y, usable.height, margin.top, margin.bottom);

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

	view_set_shade(view, false);

	struct wlr_box geo;
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

	view_set_shade(view, false);

	struct wlr_box geo = view->pending;
	snap_shrink_to_next_edge(view, direction, &geo);
	view_move_resize(view, geo);
}

enum view_axis
view_axis_parse(const char *direction)
{
	if (!direction) {
		return VIEW_AXIS_INVALID;
	}
	if (!strcasecmp(direction, "horizontal")) {
		return VIEW_AXIS_HORIZONTAL;
	} else if (!strcasecmp(direction, "vertical")) {
		return VIEW_AXIS_VERTICAL;
	} else if (!strcasecmp(direction, "both")) {
		return VIEW_AXIS_BOTH;
	} else if (!strcasecmp(direction, "none")) {
		return VIEW_AXIS_NONE;
	} else {
		return VIEW_AXIS_INVALID;
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

enum view_placement_policy
view_placement_parse(const char *policy)
{
	if (!policy) {
		return LAB_PLACE_CENTER;
	}

	if (!strcasecmp(policy, "automatic")) {
		return LAB_PLACE_AUTOMATIC;
	} else if (!strcasecmp(policy, "cursor")) {
		return LAB_PLACE_CURSOR;
	} else if (!strcasecmp(policy, "center")) {
		return LAB_PLACE_CENTER;
	} else if (!strcasecmp(policy, "cascade")) {
		return LAB_PLACE_CASCADE;
	}

	return LAB_PLACE_INVALID;
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

	view_set_shade(view, false);

	if (across_outputs && view->tiled == edge && view->maximized == VIEW_AXIS_NONE) {
		/* We are already tiled for this edge; try to switch outputs */
		output = output_get_adjacent(view->output, edge, /* wrap */ false);

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
	view_notify_tiled(view);
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

	view_set_shade(view, false);

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
	view_notify_tiled(view);
	view_apply_region_geometry(view);
}

void
view_move_to_output(struct view *view, struct output *output)
{
	assert(view);

	view_invalidate_last_layout_geometry(view);
	view_set_output(view, output);
	if (view_is_floating(view)) {
		struct wlr_box output_area = output_usable_area_in_layout_coords(output);
		view->pending.x = output_area.x;
		view->pending.y = output_area.y;
		view_place_by_policy(view,
				/* allow_cursor */ false, rc.placement_policy);
	} else if (view->fullscreen) {
		view_apply_fullscreen_geometry(view);
	} else if (view->maximized != VIEW_AXIS_NONE) {
		view_apply_maximized_geometry(view);
	} else if (view->tiled) {
		view_apply_tiled_geometry(view);
	} else if (view->tiled_region) {
		struct region *region = regions_from_name(view->tiled_region->name, output);
		view_snap_to_region(view, region, /*store_natural_geometry*/ false);
	}
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
	desktop_update_top_layer_visiblity(view->server);
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
	desktop_update_top_layer_visiblity(view->server);
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
	if (!title) {
		return;
	}
	ssd_update_title(view->ssd);

	wl_signal_emit_mutable(&view->events.new_title, NULL);
}

void
view_update_app_id(struct view *view)
{
	assert(view);
	const char *app_id = view_get_string_prop(view, "app_id");
	if (!app_id) {
		return;
	}

	if (view->ssd_enabled) {
		ssd_update_window_icon(view->ssd);
	}

	wl_signal_emit_mutable(&view->events.new_app_id, NULL);
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

int
view_get_min_width(void)
{
	int button_count_left = wl_list_length(&rc.title_buttons_left);
	int button_count_right =  wl_list_length(&rc.title_buttons_right);
	return (rc.theme->window_button_width * (button_count_left + button_count_right)) +
		(rc.theme->window_button_spacing * MAX((button_count_right - 1), 0)) +
		(rc.theme->window_button_spacing * MAX((button_count_left - 1), 0)) +
		(2 * rc.theme->window_titlebar_padding_width);
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
view_set_shade(struct view *view, bool shaded)
{
	assert(view);

	if (view->shaded == shaded) {
		return;
	}

	/* Views without a title-bar or SSD cannot be shaded */
	if (shaded && (!view->ssd || view->ssd_titlebar_hidden)) {
		return;
	}

	/* If this window is being resized, cancel the resize when shading */
	if (shaded && view->server->input_mode == LAB_INPUT_STATE_RESIZE) {
		interactive_cancel(view);
	}

	view->shaded = shaded;
	ssd_enable_shade(view->ssd, view->shaded);
	wlr_scene_node_set_enabled(view->scene_node, !view->shaded);

	if (view->impl->shade) {
		view->impl->shade(view, shaded);
	}
}

void
view_init(struct view *view)
{
	assert(view);

	wl_signal_init(&view->events.new_app_id);
	wl_signal_init(&view->events.new_title);
	wl_signal_init(&view->events.new_outputs);
	wl_signal_init(&view->events.maximized);
	wl_signal_init(&view->events.minimized);
	wl_signal_init(&view->events.fullscreened);
	wl_signal_init(&view->events.activated);
}

void
view_destroy(struct view *view)
{
	assert(view);
	struct server *server = view->server;

	snap_constraints_invalidate(view);

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

	if (view->foreign_toplevel) {
		foreign_toplevel_destroy(view->foreign_toplevel);
		view->foreign_toplevel = NULL;
	}

	if (server->grabbed_view == view) {
		/* Application got killed while moving around */
		server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
		server->grabbed_view = NULL;
		overlay_hide(&server->seat);
	}

	if (server->active_view == view) {
		server->active_view = NULL;
	}

	if (server->session_lock_manager->last_active_view == view) {
		server->session_lock_manager->last_active_view = NULL;
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

	menu_on_view_destroy(view);

	/*
	 * Destroy the view's scene tree. View methods assume this is non-NULL,
	 * so we should avoid any calls to those between this and freeing the
	 * view.
	 */
	if (view->scene_tree) {
		wlr_scene_node_destroy(&view->scene_tree->node);
		view->scene_tree = NULL;
	}

	/* Remove view from server->views */
	wl_list_remove(&view->link);
	free(view);

	cursor_update_focus(server);
}
