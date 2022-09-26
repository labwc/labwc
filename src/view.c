// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <xcb/xcb_icccm.h>
#include "common/scene-helpers.h"
#include "labwc.h"
#include "ssd.h"
#include "menu/menu.h"
#include "workspaces.h"

#define LAB_FALLBACK_WIDTH 640
#define LAB_FALLBACK_HEIGHT 480
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/**
 * All view_apply_xxx_geometry() functions must *not* modify
 * any state besides repositioning or resizing the view.
 *
 * They may be called repeatably during output layout changes.
 */

enum view_edge {
	VIEW_EDGE_INVALID = 0,

	VIEW_EDGE_LEFT,
	VIEW_EDGE_RIGHT,
	VIEW_EDGE_UP,
	VIEW_EDGE_DOWN,
	VIEW_EDGE_CENTER,
};

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
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	if (usable.height == output->wlr_output->height
			&& output->wlr_output->scale != 1) {
		usable.height /= output->wlr_output->scale;
	}
	if (usable.width == output->wlr_output->width
			&& output->wlr_output->scale != 1) {
		usable.width /= output->wlr_output->scale;
	}

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
	struct wlr_box dst = {
		.x = x_offset + usable.x + view->margin.left,
		.y = y_offset + usable.y + view->margin.top,
		.width = base_width - view->margin.left - view->margin.right,
		.height = base_height - view->margin.top - view->margin.bottom,
	};

	return dst;
}

static void
_view_set_activated(struct view *view, bool activated)
{
	if (view->ssd.tree) {
		ssd_set_active(view, activated);
	}
	if (view->impl->set_activated) {
		view->impl->set_activated(view, activated);
	}
	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_set_activated(
			view->toplevel_handle, activated);
	}
}

void
view_set_activated(struct view *view)
{
	assert(view);

	struct view *last = view->server->focused_view;
	if (last == view) {
		return;
	}
	if (last) {
		_view_set_activated(last, false);
	}
	_view_set_activated(view, true);
	view->server->focused_view = view;
}

void
view_close(struct view *view)
{
	if (view->impl->close) {
		view->impl->close(view);
	}
}

void
view_move(struct view *view, int x, int y)
{
	if (view->impl->move) {
		view->impl->move(view, x, y);
	}
}

void
view_moved(struct view *view)
{
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	view_discover_output(view);
	ssd_update_geometry(view);
	cursor_update_focus(view->server);
}

/* N.B. Use view_move() if not resizing. */
void
view_move_resize(struct view *view, struct wlr_box geo)
{
	if (view->impl->configure) {
		view->impl->configure(view, geo);
	}
}

#define MIN_VIEW_WIDTH (100)
#define MIN_VIEW_HEIGHT (60)

#if HAVE_XWAYLAND
static int
round_to_increment(int val, int base, int inc)
{
	if (base < 0 || inc <= 0)
		return val;
	return base + (val - base + inc / 2) / inc * inc;
}
#endif

void
view_adjust_size(struct view *view, int *w, int *h)
{
	int min_width = MIN_VIEW_WIDTH;
	int min_height = MIN_VIEW_HEIGHT;
#if HAVE_XWAYLAND
	if (view->type == LAB_XWAYLAND_VIEW) {
		xcb_size_hints_t *hints = view->xwayland_surface->size_hints;

		/*
		 * Honor size increments from WM_SIZE_HINTS. Typically, X11
		 * terminal emulators will use WM_SIZE_HINTS to make sure that
		 * the terminal is resized to a width/height evenly divisible by
		 * the cell (character) size.
		 */
		if (hints) {
			*w = round_to_increment(*w, hints->base_width,
				hints->width_inc);
			*h = round_to_increment(*h, hints->base_height,
				hints->height_inc);

			min_width = MAX(1, hints->min_width);
			min_height = MAX(1, hints->min_height);
		}
	}
#endif
	*w = MAX(*w, min_width);
	*h = MAX(*h, min_height);
}

void
view_minimize(struct view *view, bool minimized)
{
	if (view->minimized == minimized) {
		return;
	}
	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_set_minimized(
			view->toplevel_handle, minimized);
	}
	view->minimized = minimized;
	if (minimized) {
		view->impl->unmap(view);
		desktop_move_to_back(view);
		_view_set_activated(view, false);
		if (view == view->server->focused_view) {
			/*
			 * Prevents clearing the active view when
			 * we don't actually have keyboard focus.
			 *
			 * This may happen when using a custom mouse
			 * focus configuration or by using the foreign
			 * toplevel protocol via some panel.
			 */
			view->server->focused_view = NULL;
		}
	} else {
		view->impl->map(view);
	}
}

/* view_wlr_output - return the output that a view is mostly on */
struct wlr_output *
view_wlr_output(struct view *view)
{
	double closest_x, closest_y;
	struct wlr_output *wlr_output = NULL;
	wlr_output_layout_closest_point(view->server->output_layout, wlr_output,
		view->x + view->w / 2, view->y + view->h / 2, &closest_x,
		&closest_y);
	wlr_output = wlr_output_layout_output_at(view->server->output_layout,
		closest_x, closest_y);
	return wlr_output;
}

static struct output *
view_output(struct view *view)
{
	struct wlr_output *wlr_output = view_wlr_output(view);
	return output_from_wlr_output(view->server, wlr_output);
}

static bool
view_compute_centered_position(struct view *view, int w, int h, int *x, int *y)
{
	struct output *output = view_output(view);
	if (!output) {
		return false;
	}
	struct wlr_output *wlr_output = output->wlr_output;
	if (!wlr_output) {
		return false;
	}

	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	int width = w + view->margin.left + view->margin.right;
	int height = h + view->margin.top + view->margin.bottom;
	*x = usable.x + (usable.width - width) / 2;
	*y = usable.y + (usable.height - height) / 2;

	/* If view is bigger than usable area, just top/left align it */
	if (*x < 0) {
		*x = 0;
	}
	if (*y < 0) {
		*y = 0;
	}

#if HAVE_XWAYLAND
	/* TODO: refactor xwayland.c functions to get rid of this */
	if (view->type == LAB_XWAYLAND_VIEW) {
		*x += view->margin.left;
		*y += view->margin.top;
	}
#endif

	return true;
}

static void
set_fallback_geometry(struct view *view)
{
	view->natural_geometry.width = LAB_FALLBACK_WIDTH;
	view->natural_geometry.height = LAB_FALLBACK_HEIGHT;
	view_compute_centered_position(view,
		view->natural_geometry.width,
		view->natural_geometry.height,
		&view->natural_geometry.x,
		&view->natural_geometry.y);
}

#undef LAB_FALLBACK_WIDTH
#undef LAB_FALLBACK_HEIGHT

static void
view_store_natural_geometry(struct view *view)
{
	/**
	 * If an application was started maximized or fullscreened, its
	 * natural_geometry width/height may still be zero in which case we set
	 * some fallback values. This is the case with foot and Qt applications.
	 */
	if (!view->w || !view->h) {
		set_fallback_geometry(view);
	} else {
		view->natural_geometry.x = view->x;
		view->natural_geometry.y = view->y;
		view->natural_geometry.width = view->w;
		view->natural_geometry.height = view->h;
	}
}

void
view_center(struct view *view)
{
	int x, y;
	if (view_compute_centered_position(view, view->w, view->h, &x, &y)) {
		view_move(view, x, y);
	}
}

static void
view_apply_tiled_geometry(struct view *view, struct output *output)
{
	assert(view->tiled);
	if (!output) {
		output = view_output(view);
	}
	if (!output) {
		wlr_log(WLR_ERROR, "Can't tile: no output");
		return;
	}

	struct wlr_box dst = view_get_edge_snap_box(view, output, view->tiled);
	if (view->w == dst.width && view->h == dst.height) {
		/* move horizontally/vertically without changing size */
		view_move(view, dst.x, dst.y);
	} else {
		view_move_resize(view, dst);
	}
}

static void
view_apply_fullscreen_geometry(struct view *view, struct wlr_output *wlr_output)
{
	struct output *output =
		output_from_wlr_output(view->server, wlr_output);
	struct wlr_box box = { 0 };
	wlr_output_effective_resolution(wlr_output, &box.width, &box.height);
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	box.x -= ox;
	box.y -= oy;
	view_move_resize(view, box);
}

static void
view_apply_maximized_geometry(struct view *view)
{
	/*
	 * The same code handles both initial maximize and re-maximize
	 * to account for layout changes.  In either case, view_output()
	 * gives the output closest to the current geometry (which may
	 * be different from the output originally maximized onto).
	 * view_output() can return NULL if all outputs are disabled.
	 */
	struct output *output = view_output(view);
	if (!output) {
		return;
	}
	struct wlr_box box = output_usable_area_in_layout_coords(output);
	if (box.height == output->wlr_output->height
			&& output->wlr_output->scale != 1) {
		box.height /= output->wlr_output->scale;
	}
	if (box.width == output->wlr_output->width
			&& output->wlr_output->scale != 1) {
		box.width /= output->wlr_output->scale;
	}

	if (view->ssd.enabled) {
		struct border border = ssd_thickness(view);
		box.x += border.left;
		box.y += border.top;
		box.width -= border.right + border.left;
		box.height -= border.top + border.bottom;
	}
	view_move_resize(view, box);
}

static void
view_apply_unmaximized_geometry(struct view *view)
{
	struct wlr_output_layout *layout = view->server->output_layout;
	if (wlr_output_layout_intersects(layout, NULL,
			&view->natural_geometry)) {
		/* restore to original geometry */
		view_move_resize(view, view->natural_geometry);
	} else {
		/* reposition if original geometry is offscreen */
		struct wlr_box box = view->natural_geometry;
		if (view_compute_centered_position(view, box.width, box.height,
				&box.x, &box.y)) {
			view_move_resize(view, box);
		}
	}
}

void
view_maximize(struct view *view, bool maximize)
{
	if (view->maximized == maximize) {
		return;
	}
	if (view->fullscreen) {
		return;
	}
	if (view->impl->maximize) {
		view->impl->maximize(view, maximize);
	}
	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_set_maximized(
			view->toplevel_handle, maximize);
	}
	if (maximize) {
		interactive_end(view);
		if (!view->tiled) {
			view_store_natural_geometry(view);
		}
		view_apply_maximized_geometry(view);
		view->maximized = true;
	} else {
		/* unmaximize */
		if (view->tiled) {
			view_apply_tiled_geometry(view, NULL);
		} else {
			view_apply_unmaximized_geometry(view);
		}
		view->maximized = false;
	}
}

void
view_toggle_maximize(struct view *view)
{
	view_maximize(view, !view->maximized);
}

void
view_toggle_decorations(struct view *view)
{
	if (!view->fullscreen) {
		view->ssd.enabled = !view->ssd.enabled;
		ssd_update_geometry(view);
		if (view->maximized) {
			view_apply_maximized_geometry(view);
		} else if (view->tiled) {
			view_apply_tiled_geometry(view, NULL);
		}
	}
}

static bool
is_always_on_top(struct view *view)
{
	return view->scene_tree->node.parent ==
		view->server->view_tree_always_on_top;
}

void
view_toggle_always_on_top(struct view *view)
{
	if (is_always_on_top(view)) {
		view->workspace = view->server->workspace_current;
		wlr_scene_node_reparent(&view->scene_tree->node, view->workspace->tree);
	} else {
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->server->view_tree_always_on_top);
	}
}

void
view_set_decorations(struct view *view, bool decorations)
{
	if (view->ssd.enabled != decorations && !view->fullscreen) {
		view->ssd.enabled = decorations;
		ssd_update_geometry(view);
		if (view->maximized) {
			view_apply_maximized_geometry(view);
		} else if (view->tiled) {
			view_apply_tiled_geometry(view, NULL);
		}
	}
}

void
view_toggle_fullscreen(struct view *view)
{
	view_set_fullscreen(view, !view->fullscreen, NULL);
}

void
view_set_fullscreen(struct view *view, bool fullscreen,
		struct wlr_output *wlr_output)
{
	if (fullscreen != !view->fullscreen) {
		return;
	}
	if (view->impl->set_fullscreen) {
		view->impl->set_fullscreen(view, fullscreen);
	}
	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_set_fullscreen(
			view->toplevel_handle, fullscreen);
	}
	if (!wlr_output) {
		wlr_output = view_wlr_output(view);
	}
	if (fullscreen) {
		interactive_end(view);
		if (!view->maximized && !view->tiled) {
			view_store_natural_geometry(view);
		}
		view->fullscreen = wlr_output;
		view_apply_fullscreen_geometry(view, view->fullscreen);
	} else {
		/* restore to normal */
		if (view->maximized) {
			view_apply_maximized_geometry(view);
		} else if (view->tiled) {
			view_apply_tiled_geometry(view, NULL);
		} else {
			view_apply_unmaximized_geometry(view);
		}
		view->fullscreen = false;
	}

	/* Show fullscreen views above top-layer */
	struct output *output =
		output_from_wlr_output(view->server, wlr_output);
	uint32_t top = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	wlr_scene_node_set_enabled(&output->layer_tree[top]->node, !fullscreen);
}

void
view_adjust_for_layout_change(struct view *view)
{
	struct wlr_output_layout *layout = view->server->output_layout;
	if (view->fullscreen) {
		if (wlr_output_layout_get(layout, view->fullscreen)) {
			/* recompute fullscreen geometry */
			view_apply_fullscreen_geometry(view, view->fullscreen);
		} else {
			/* output is gone, exit fullscreen */
			view_set_fullscreen(view, false, NULL);
		}
	} else if (view->maximized) {
		/* recompute maximized geometry */
		view_apply_maximized_geometry(view);
	} else if (view->tiled) {
		/* recompute tiled geometry */
		view_apply_tiled_geometry(view, NULL);
	} else {
		/* reposition view if it's offscreen */
		struct wlr_box box = { view->x, view->y, view->w, view->h };
		if (!wlr_output_layout_intersects(layout, NULL, &box)) {
			view_center(view);
		}
	}
}

static void
view_output_enter(struct view *view, struct wlr_output *wlr_output)
{
	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_output_enter(
			view->toplevel_handle, wlr_output);
	}
}

static void
view_output_leave(struct view *view, struct wlr_output *wlr_output)
{
	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_output_leave(
			view->toplevel_handle, wlr_output);
	}
}

/*
 * At present, a view can only 'enter' one output at a time, although the view
 * may span multiple outputs. Ideally we would handle multiple outputs, but
 * this method is the simplest form of what we want.
 */
void
view_discover_output(struct view *view)
{
	struct output *old_output = view->output;
	struct output *new_output = view_output(view);
	if (old_output != new_output) {
		view->output = new_output;
		if (new_output) {
			view_output_enter(view, new_output->wlr_output);
		}
		if (old_output) {
			view_output_leave(view, old_output->wlr_output);
		}
	}
}

void
view_on_output_destroy(struct view *view)
{
	view_output_leave(view, view->output->wlr_output);
	view->output = NULL;
}

void
view_move_to_edge(struct view *view, const char *direction)
{
	if (!view) {
		wlr_log(WLR_ERROR, "no view");
		return;
	}
	struct output *output = view_output(view);
	if (!output) {
		wlr_log(WLR_ERROR, "no output");
		return;
	}
	if (!direction) {
		wlr_log(WLR_ERROR, "invalid edge");
		return;
	}
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	if (usable.height == output->wlr_output->height
			&& output->wlr_output->scale != 1) {
		usable.height /= output->wlr_output->scale;
	}
	if (usable.width == output->wlr_output->width
			&& output->wlr_output->scale != 1) {
		usable.width /= output->wlr_output->scale;
	}

	int x = 0, y = 0;
	if (!strcasecmp(direction, "left")) {
		x = usable.x + view->margin.left + rc.gap;
		y = view->y;
	} else if (!strcasecmp(direction, "up")) {
		x = view->x;
		y = usable.y + view->margin.top + rc.gap;
	} else if (!strcasecmp(direction, "right")) {
		x = usable.x + usable.width - view->w - view->margin.right
			- rc.gap;
		y = view->y;
	} else if (!strcasecmp(direction, "down")) {
		x = view->x;
		y = usable.y + usable.height - view->h - view->margin.bottom
			- rc.gap;
	} else {
		wlr_log(WLR_ERROR, "invalid edge");
		return;
	}
	view_move(view, x, y);
}

static enum view_edge
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
view_snap_to_edge(struct view *view, const char *direction)
{
	if (!view) {
		wlr_log(WLR_ERROR, "no view");
		return;
	}
	if (view->fullscreen) {
		return;
	}
	struct output *output = view_output(view);
	if (!output) {
		wlr_log(WLR_ERROR, "no output");
		return;
	}
	enum view_edge edge = view_edge_parse(direction);
	if (edge == VIEW_EDGE_INVALID) {
		wlr_log(WLR_ERROR, "invalid edge");
		return;
	}

	if (view->tiled == edge) {
		/* We are already tiled for this edge and thus should switch outputs */
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
		if (new_output && new_output != current_output) {
			/* Move to next output */
			edge = view_edge_invert(edge);
			output = output_from_wlr_output(view->server, new_output);
		} else {
			/* No more output to move to */
			return;
		}
	}

	if (view->maximized) {
		/* Unmaximize + keep using existing natural_geometry */
		view_maximize(view, false);
	} else if (!view->tiled) {
		/* store current geometry as new natural_geometry */
		view_store_natural_geometry(view);
	}
	view->tiled = edge;
	view_apply_tiled_geometry(view, output);
}

const char *
view_get_string_prop(struct view *view, const char *prop)
{
	if (view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, prop);
	}
	return "";
}

void
view_update_title(struct view *view)
{
	const char *title = view_get_string_prop(view, "title");
	if (!view->toplevel_handle || !title) {
		return;
	}
	ssd_update_title(view);
	wlr_foreign_toplevel_handle_v1_set_title(view->toplevel_handle, title);
}

void
view_update_app_id(struct view *view)
{
	const char *app_id = view_get_string_prop(view, "app_id");
	if (!view->toplevel_handle || !app_id) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_app_id(
		view->toplevel_handle, app_id);
}

void
view_destroy(struct view *view)
{
	struct server *server = view->server;
	bool need_cursor_update = false;

	if (view->toplevel_handle) {
		wlr_foreign_toplevel_handle_v1_destroy(view->toplevel_handle);
	}

	if (server->grabbed_view == view) {
		/* Application got killed while moving around */
		server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
		server->grabbed_view = NULL;
		need_cursor_update = true;
	}

	if (server->focused_view == view) {
		server->focused_view = NULL;
		need_cursor_update = true;
	}

	if (server->seat.pressed.view == view) {
		seat_reset_pressed(&server->seat);
	}

	osd_on_view_destroy(view);

	if (view->scene_tree) {
		ssd_destroy(view);
		wlr_scene_node_destroy(&view->scene_tree->node);
		view->scene_tree = NULL;
	}

	/*
	 * The layer-shell top-layer is disabled when an application is running
	 * in fullscreen mode, so if that's the case, we have to re-enable it
	 * here.
	 */
	if (view->fullscreen) {
		struct output *output =
			output_from_wlr_output(server, view->fullscreen);
		uint32_t top = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
		wlr_scene_node_set_enabled(&output->layer_tree[top]->node, true);
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
