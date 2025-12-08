// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_dialog_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_toplevel_icon_v1.h>
#include "buffer.h"
#include "common/array.h"
#include "common/box.h"
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "decorations.h"
#include "foreign-toplevel/foreign.h"
#include "labwc.h"
#include "menu/menu.h"
#include "node.h"
#include "output.h"
#include "snap-constraints.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"
#include "workspaces.h"

#define LAB_XDG_SHELL_VERSION 6
#define CONFIGURE_TIMEOUT_MS 100

static struct xdg_toplevel_view *
xdg_toplevel_view_from_view(struct view *view)
{
	assert(view->type == LAB_XDG_SHELL_VIEW);
	return (struct xdg_toplevel_view *)view;
}

struct wlr_xdg_surface *
xdg_surface_from_view(struct view *view)
{
	assert(view->type == LAB_XDG_SHELL_VIEW);
	struct xdg_toplevel_view *xdg_toplevel_view =
		(struct xdg_toplevel_view *)view;
	assert(xdg_toplevel_view->xdg_surface);
	return xdg_toplevel_view->xdg_surface;
}

static struct wlr_xdg_toplevel *
xdg_toplevel_from_view(struct view *view)
{
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
	assert(xdg_surface->toplevel);
	return xdg_surface->toplevel;
}

static struct view_size_hints
xdg_toplevel_view_get_size_hints(struct view *view)
{
	assert(view);

	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	struct wlr_xdg_toplevel_state *state = &toplevel->current;

	return (struct view_size_hints){
		.min_width = state->min_width,
		.min_height = state->min_height,
	};
}

static bool
xdg_toplevel_view_contains_window_type(struct view *view,
		enum lab_window_type window_type)
{
	assert(view);

	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	struct wlr_xdg_toplevel_state *state = &toplevel->current;
	bool is_dialog = (state->min_width != 0 && state->min_height != 0
		&& (state->min_width == state->max_width
		|| state->min_height == state->max_height))
		|| toplevel->parent;

	switch (window_type) {
	case LAB_WINDOW_TYPE_NORMAL:
		return !is_dialog;
	case LAB_WINDOW_TYPE_DIALOG:
		return is_dialog;
	default:
		return false;
	}
}

static void
handle_new_popup(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_view *xdg_toplevel_view =
		wl_container_of(listener, xdg_toplevel_view, new_popup);
	struct view *view = &xdg_toplevel_view->base;
	struct wlr_xdg_popup *wlr_popup = data;
	xdg_popup_create(view, wlr_popup);
}

static void
set_fullscreen_from_request(struct view *view,
		struct wlr_xdg_toplevel_requested *requested)
{
	if (!view->fullscreen && requested->fullscreen
			&& requested->fullscreen_output) {
		view_set_output(view, output_from_wlr_output(view->server,
			requested->fullscreen_output));
	}
	view_set_fullscreen(view, requested->fullscreen);
}

static void
do_late_positioning(struct view *view)
{
	struct server *server = view->server;
	if (server->input_mode == LAB_INPUT_STATE_MOVE
			&& view == server->grabbed_view) {
		/* Reposition the view while anchoring it to cursor */
		interactive_anchor_to_cursor(server, &view->pending);
	} else {
		/* TODO: smart placement? */
		view_compute_centered_position(view, NULL,
			view->pending.width, view->pending.height,
			&view->pending.x, &view->pending.y);
	}
}

static void
disable_fullscreen_bg(struct view *view)
{
	struct xdg_toplevel_view *xdg_view = xdg_toplevel_view_from_view(view);
	if (xdg_view->fullscreen_bg) {
		wlr_scene_node_set_enabled(&xdg_view->fullscreen_bg->node, false);
	}
}

/*
 * Centers any fullscreen view smaller than the full output size.
 * This should be called immediately before view_moved().
 */
static void
center_fullscreen_if_needed(struct view *view)
{
	if (!view->fullscreen || !output_is_usable(view->output)) {
		disable_fullscreen_bg(view);
		return;
	}

	struct wlr_box output_box = {0};
	wlr_output_layout_get_box(view->server->output_layout,
		view->output->wlr_output, &output_box);
	box_center(view->current.width, view->current.height, &output_box,
		&output_box, &view->current.x, &view->current.y);

	/* Reset pending x/y to computed position also */
	view->pending.x = view->current.x;
	view->pending.y = view->current.y;

	if (view->current.width >= output_box.width
			&& view->current.width >= output_box.height) {
		disable_fullscreen_bg(view);
		return;
	}

	struct xdg_toplevel_view *xdg_view = xdg_toplevel_view_from_view(view);
	if (!xdg_view->fullscreen_bg) {
		const float black[4] = {0, 0, 0, 1};
		xdg_view->fullscreen_bg =
			wlr_scene_rect_create(view->scene_tree, 0, 0, black);
		wlr_scene_node_lower_to_bottom(&xdg_view->fullscreen_bg->node);
	}

	wlr_scene_node_set_position(&xdg_view->fullscreen_bg->node,
		output_box.x - view->current.x, output_box.y - view->current.y);
	wlr_scene_rect_set_size(xdg_view->fullscreen_bg,
		output_box.width, output_box.height);
	wlr_scene_node_set_enabled(&xdg_view->fullscreen_bg->node, true);
}

/* TODO: reorder so this forward declaration isn't needed */
static void set_pending_configure_serial(struct view *view, uint32_t serial);

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	assert(view->surface);

	if (xdg_surface->initial_commit) {
		uint32_t serial =
			wlr_xdg_surface_schedule_configure(xdg_surface);
		if (serial > 0) {
			set_pending_configure_serial(view, serial);
		}

		uint32_t wm_caps = WLR_XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU
			| WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE
			| WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN
			| WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
		wlr_xdg_toplevel_set_wm_capabilities(toplevel, wm_caps);

		if (view->output) {
			wlr_xdg_toplevel_set_bounds(toplevel,
				view->output->usable_area.width,
				view->output->usable_area.height);
		}

		/*
		 * Handle initial fullscreen/maximize requests immediately after
		 * scheduling the initial configure event (before it is sent) in
		 * order to send the correct size and avoid flicker.
		 *
		 * In normal (non-fullscreen/maximized) cases, the initial
		 * configure event is sent with a zero size, which requests the
		 * application to choose its own size.
		 */
		if (toplevel->requested.fullscreen) {
			set_fullscreen_from_request(view, &toplevel->requested);
		}
		if (toplevel->requested.maximized) {
			view_maximize(view, VIEW_AXIS_BOTH,
				/*store_natural_geometry*/ true);
		}
		return;
	}

	struct wlr_box size = xdg_surface->geometry;
	bool update_required = false;

	/*
	 * If we didn't know the natural size when leaving fullscreen or
	 * unmaximizing, then the pending size will be 0x0. In this case,
	 * the pending x/y is also unset and we still need to position
	 * the window.
	 */
	if (wlr_box_empty(&view->pending) && !wlr_box_empty(&size)) {
		view->pending.width = size.width;
		view->pending.height = size.height;
		do_late_positioning(view);
		update_required = true;
	}

	/*
	 * Qt applications occasionally fail to call set_window_geometry
	 * after a configure request, but do correctly update the actual
	 * surface extent. This results in a mismatch between the window
	 * decorations (which follow the logical geometry) and the visual
	 * size of the client area. As a workaround, we try to detect
	 * this case and ignore the out-of-date window geometry.
	 */
	if (size.width != view->pending.width
			|| size.height != view->pending.height) {
		/*
		 * Not using wlr_surface_get_extend() since Thunderbird
		 * sometimes resizes the window geometry and the toplevel
		 * surface size, but not the subsurface size (see #2183).
		 */
		struct wlr_box extent = {
			.width = view->surface->current.width,
			.height = view->surface->current.height,
		};
		if (extent.width == view->pending.width
				&& extent.height == view->pending.height) {
			wlr_log(WLR_DEBUG, "window geometry for client (%s) "
				"appears to be incorrect - ignoring",
				view->app_id);
			size = extent; /* Use surface extent instead */
		}
	}

	struct wlr_box *current = &view->current;
	if (current->width != size.width || current->height != size.height) {
		update_required = true;
	}

	uint32_t serial = view->pending_configure_serial;
	if (serial > 0 && serial == xdg_surface->current.configure_serial) {
		assert(view->pending_configure_timeout);
		wl_event_source_remove(view->pending_configure_timeout);
		view->pending_configure_serial = 0;
		view->pending_configure_timeout = NULL;
		update_required = true;
	}

	if (update_required) {
		view_impl_apply_geometry(view, size.width, size.height);
		center_fullscreen_if_needed(view);
		view_moved(view);

		/*
		 * Some views (e.g., terminals that scale as multiples of rows
		 * and columns, or windows that impose a fixed aspect ratio),
		 * may respond to a resize but alter the width or height. When
		 * this happens, view->pending will be out of sync with the
		 * actual geometry (size *and* position, depending on the edge
		 * from which the resize was attempted). When no other
		 * configure is pending, re-sync the pending geometry with the
		 * actual view.
		 */
		if (!view->pending_configure_serial) {
			snap_constraints_update(view);
			view->pending = view->current;

			/*
			 * wlroots retains the size set by any call to
			 * wlr_xdg_toplevel_set_size and will send the retained
			 * values with every subsequent configure request. If a
			 * client has resized itself in the meantime, a
			 * configure request that sends the now-outdated size
			 * may prompt the client to resize itself unexpectedly.
			 *
			 * Calling wlr_xdg_toplevel_set_size to update the
			 * value held by wlroots is undesirable here, because
			 * that will trigger another configure event and we
			 * don't want to get stuck in a request-response loop.
			 * Instead, just manipulate the dimensions that *would*
			 * be adjusted by the call, so the right values will
			 * apply next time.
			 *
			 * This is not ideal, but it is the cleanest option.
			 */
			toplevel->scheduled.width = view->current.width;
			toplevel->scheduled.height = view->current.height;
		}
	}
}

static int
handle_configure_timeout(void *data)
{
	struct view *view = data;
	assert(view->pending_configure_serial > 0);
	assert(view->pending_configure_timeout);

	wlr_log(WLR_INFO, "client (%s) did not respond to configure request "
		"in %d ms", view->app_id, CONFIGURE_TIMEOUT_MS);

	wl_event_source_remove(view->pending_configure_timeout);
	view->pending_configure_serial = 0;
	view->pending_configure_timeout = NULL;

	/*
	 * No need to do anything else if the view is just being slow to
	 * map - the map handler will take care of the positioning.
	 */
	if (!view->mapped) {
		return 0; /* ignored per wl_event_loop docs */
	}

	bool empty_pending = wlr_box_empty(&view->pending);
	if (empty_pending || view->pending.x != view->current.x
			|| view->pending.y != view->current.y) {
		/*
		 * This is a pending move + resize and the client is
		 * taking too long to respond to the resize. Apply the
		 * move now (while keeping the current size) so that the
		 * desktop doesn't appear unresponsive.
		 *
		 * We do not use view_impl_apply_geometry() here since
		 * in this case we prefer to always put the top-left
		 * corner of the view at the desired position rather
		 * than anchoring some other edge or corner.
		 *
		 * Corner case: we may get here with an empty pending
		 * geometry in the case of an initially-maximized view
		 * which is taking a long time to un-maximize (seen for
		 * example with Thunderbird on slow machines). In that
		 * case we have no great options (we can't center the
		 * view since we don't know the un-maximized size yet),
		 * so set a fallback position.
		 */
		if (empty_pending) {
			wlr_log(WLR_INFO, "using fallback position");
			view->pending.x = VIEW_FALLBACK_X;
			view->pending.y = VIEW_FALLBACK_Y;
			/* At least try to keep it on the same output */
			if (output_is_usable(view->output)) {
				struct wlr_box box =
					output_usable_area_in_layout_coords(view->output);
				view->pending.x += box.x;
				view->pending.y += box.y;
			}
		}
		view->current.x = view->pending.x;
		view->current.y = view->pending.y;
	}

	center_fullscreen_if_needed(view);
	view_moved(view);

	/* Re-sync pending view with current state */
	snap_constraints_update(view);
	view->pending = view->current;

	return 0; /* ignored per wl_event_loop docs */
}

static void
set_pending_configure_serial(struct view *view, uint32_t serial)
{
	view->pending_configure_serial = serial;
	if (!view->pending_configure_timeout) {
		view->pending_configure_timeout =
			wl_event_loop_add_timer(view->server->wl_event_loop,
				handle_configure_timeout, view);
	}
	wl_event_source_timer_update(view->pending_configure_timeout,
		CONFIGURE_TIMEOUT_MS);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	assert(view->type == LAB_XDG_SHELL_VIEW);
	struct xdg_toplevel_view *xdg_toplevel_view =
		xdg_toplevel_view_from_view(view);

	struct wlr_xdg_popup *popup, *tmp;
	wl_list_for_each_safe(popup, tmp, &xdg_toplevel_view->xdg_surface->popups, link) {
		wlr_xdg_popup_destroy(popup);
	}

	xdg_toplevel_view->xdg_surface->data = NULL;
	xdg_toplevel_view->xdg_surface = NULL;

	/* Remove xdg-shell view specific listeners */
	wl_list_remove(&xdg_toplevel_view->set_app_id.link);
	wl_list_remove(&xdg_toplevel_view->request_show_window_menu.link);
	wl_list_remove(&xdg_toplevel_view->new_popup.link);
	wl_list_remove(&view->commit.link);

	if (view->pending_configure_timeout) {
		wl_event_source_remove(view->pending_configure_timeout);
		view->pending_configure_timeout = NULL;
	}

	view_destroy(view);
}

static void
handle_request_move(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provided serial against a list of button press serials sent to
	 * this client, to prevent the client from requesting this whenever they
	 * want.
	 */
	struct view *view = wl_container_of(listener, view, request_move);
	if (view == view->server->seat.pressed.ctx.view) {
		interactive_begin(view, LAB_INPUT_STATE_MOVE, LAB_EDGE_NONE);
	}
}

static void
handle_request_resize(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provided serial against a list of button press serials sent to
	 * this client, to prevent the client from requesting this whenever they
	 * want.
	 */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_resize);
	if (view == view->server->seat.pressed.ctx.view) {
		interactive_begin(view, LAB_INPUT_STATE_RESIZE, event->edges);
	}
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_minimize);
	view_minimize(view, xdg_toplevel_from_view(view)->requested.minimized);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_maximize);
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);

	if (!toplevel->base->initialized) {
		/*
		 * Do nothing if we have not received the initial commit yet.
		 * We will maximize the view in the commit handler.
		 */
		return;
	}

	if (!view->mapped && !view->output) {
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
	bool maximized = toplevel->requested.maximized;
	view_maximize(view, maximized ? VIEW_AXIS_BOTH : VIEW_AXIS_NONE,
		/*store_natural_geometry*/ true);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_fullscreen);
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);

	if (!toplevel->base->initialized) {
		/*
		 * Do nothing if we have not received the initial commit yet.
		 * We will fullscreen the view in the commit handler.
		 */
		return;
	}

	if (!view->mapped && !view->output) {
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
	set_fullscreen_from_request(view,
		&xdg_toplevel_from_view(view)->requested);
}

static void
handle_request_show_window_menu(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_view *xdg_toplevel_view = wl_container_of(
		listener, xdg_toplevel_view, request_show_window_menu);
	struct server *server = xdg_toplevel_view->base.server;

	struct menu *menu = menu_get_by_id(server, "client-menu");
	assert(menu);
	menu->triggered_by_view = &xdg_toplevel_view->base;

	struct wlr_cursor *cursor = server->seat.cursor;
	menu_open_root(menu, cursor->x, cursor->y);
}

static void
handle_set_title(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, set_title);
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);

	view_set_title(view, toplevel->title);
}

static void
handle_set_app_id(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_view *xdg_toplevel_view =
		wl_container_of(listener, xdg_toplevel_view, set_app_id);
	struct view *view = &xdg_toplevel_view->base;
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);

	view_set_app_id(view, toplevel->app_id);
}

static void
xdg_toplevel_view_configure(struct view *view, struct wlr_box geo)
{
	uint32_t serial = 0;

	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);

	/*
	 * We do not need to send a configure request unless the size
	 * changed (wayland has no notion of a global position). If the
	 * size is the same (and there is no pending configure request)
	 * then we can just move the view directly.
	 */
	if (geo.width != view->pending.width
			|| geo.height != view->pending.height) {
		if (toplevel->base->initialized) {
			serial = wlr_xdg_toplevel_set_size(toplevel, geo.width, geo.height);
		} else {
			/*
			 * This may happen, for example, when a panel resizes because a
			 * foreign-toplevel has been destroyed. This would then trigger
			 * a call to desktop_arrange_all_views() which in turn explicitly
			 * also tries to configure unmapped surfaces. This is fine when
			 * trying to resize surfaces before they are mapped but it will
			 * also try to resize surfaces which have been unmapped but their
			 * associated struct view has not been destroyed yet.
			 */
			wlr_log(WLR_DEBUG, "Preventing configure of uninitialized surface");
		}
	}

	view->pending = geo;
	if (serial > 0) {
		set_pending_configure_serial(view, serial);
	} else if (view->pending_configure_serial == 0) {
		view->current.x = geo.x;
		view->current.y = geo.y;
		/*
		 * It's a bit difficult to think of a corner case where
		 * center_fullscreen_if_needed() would actually be needed
		 * here, but including it anyway for completeness.
		 */
		center_fullscreen_if_needed(view);
		view_moved(view);
	}
}

static void
xdg_toplevel_view_close(struct view *view)
{
	wlr_xdg_toplevel_send_close(xdg_toplevel_from_view(view));
}

static void
xdg_toplevel_view_maximize(struct view *view, enum view_axis maximized)
{
	if (!xdg_toplevel_from_view(view)->base->initialized) {
		wlr_log(WLR_DEBUG, "Prevented maximize notification for a non-intialized view");
		return;
	}
	uint32_t serial = wlr_xdg_toplevel_set_maximized(
		xdg_toplevel_from_view(view), maximized == VIEW_AXIS_BOTH);
	if (serial > 0) {
		set_pending_configure_serial(view, serial);
	}
}

static void
xdg_toplevel_view_minimize(struct view *view, bool minimized)
{
	/* noop */
}

static struct view *
xdg_toplevel_view_get_parent(struct view *view)
{
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	return toplevel->parent ?
		(struct view *)toplevel->parent->base->data : NULL;
}

static struct wlr_xdg_toplevel *
top_parent_of(struct view *view)
{
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	while (toplevel->parent) {
		toplevel = toplevel->parent;
	}
	return toplevel;
}

/* Return the most senior parent (=root) view */
static struct view *
xdg_toplevel_view_get_root(struct view *view)
{
	struct wlr_xdg_toplevel *root = top_parent_of(view);
	return (struct view *)root->base->data;
}

static void
xdg_toplevel_view_append_children(struct view *self, struct wl_array *children)
{
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(self);
	struct view *view;

	wl_list_for_each_reverse(view, &self->server->views, link) {
		if (view == self) {
			continue;
		}
		if (view->type != LAB_XDG_SHELL_VIEW) {
			continue;
		}
		if (!view->mapped) {
			continue;
		}
		if (top_parent_of(view) != toplevel) {
			continue;
		}
		struct view **child = wl_array_add(children, sizeof(*child));
		*child = view;
	}
}

static bool
xdg_toplevel_view_is_modal_dialog(struct view *view)
{
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	struct wlr_xdg_dialog_v1 *dialog =
		wlr_xdg_dialog_v1_try_from_wlr_xdg_toplevel(toplevel);
	if (!dialog) {
		return false;
	}
	return dialog->modal;
}

static void
xdg_toplevel_view_set_activated(struct view *view, bool activated)
{
	if (!xdg_toplevel_from_view(view)->base->initialized) {
		wlr_log(WLR_DEBUG, "Prevented activating a non-intialized view");
		return;
	}
	uint32_t serial = wlr_xdg_toplevel_set_activated(
		xdg_toplevel_from_view(view), activated);
	if (serial > 0) {
		set_pending_configure_serial(view, serial);
	}
}

static void
xdg_toplevel_view_set_fullscreen(struct view *view, bool fullscreen)
{
	if (!xdg_toplevel_from_view(view)->base->initialized) {
		wlr_log(WLR_DEBUG, "Prevented fullscreening a non-intialized view");
		return;
	}
	uint32_t serial = wlr_xdg_toplevel_set_fullscreen(
		xdg_toplevel_from_view(view), fullscreen);
	if (serial > 0) {
		set_pending_configure_serial(view, serial);
	}
	/* Disable background fill immediately on leaving fullscreen */
	if (!fullscreen) {
		disable_fullscreen_bg(view);
	}
}

static void
xdg_toplevel_view_notify_tiled(struct view *view)
{
	/* Take no action if xdg-shell tiling is disabled */
	if (rc.snap_tiling_events_mode == LAB_TILING_EVENTS_NEVER) {
		return;
	}

	if (!xdg_toplevel_from_view(view)->base->initialized) {
		wlr_log(WLR_DEBUG, "Prevented tiling notification for a non-intialized view");
		return;
	}

	enum lab_edge edge = LAB_EDGE_NONE;

	bool want_edge = rc.snap_tiling_events_mode & LAB_TILING_EVENTS_EDGE;
	bool want_region = rc.snap_tiling_events_mode & LAB_TILING_EVENTS_REGION;

	/*
	 * Edge-snapped view are considered tiled on the snapped edge and those
	 * perpendicular to it.
	 */
	if (want_edge) {
		switch (view->tiled) {
		case LAB_EDGE_LEFT:
			edge = LAB_EDGES_EXCEPT_RIGHT;
			break;
		case LAB_EDGE_RIGHT:
			edge = LAB_EDGES_EXCEPT_LEFT;
			break;
		case LAB_EDGE_TOP:
			edge = LAB_EDGES_EXCEPT_BOTTOM;
			break;
		case LAB_EDGE_BOTTOM:
			edge = LAB_EDGES_EXCEPT_TOP;
			break;
		case LAB_EDGES_TOP_LEFT:
		case LAB_EDGES_TOP_RIGHT:
		case LAB_EDGES_BOTTOM_LEFT:
		case LAB_EDGES_BOTTOM_RIGHT:
			edge = view->tiled;
			break;
		/* TODO: LAB_EDGE_CENTER? */
		default:
			edge = LAB_EDGE_NONE;
		}
	}

	if (want_region && view->tiled_region) {
		/* Region-snapped views are considered tiled on all edges */
		edge = LAB_EDGES_ALL;
	}

	uint32_t serial =
		wlr_xdg_toplevel_set_tiled(xdg_toplevel_from_view(view), edge);
	if (serial > 0) {
		set_pending_configure_serial(view, serial);
	}
}

static void
set_initial_position(struct view *view)
{
	view_constrain_size_to_that_of_usable_area(view);

	struct view *parent = xdg_toplevel_view_get_parent(view);
	if (parent) {
		/* Child views are center-aligned relative to their parents */
		view_set_output(view, parent->output);
		view_center(view, &parent->pending);
		return;
	}

	/* All other views are placed according to a configured strategy */
	view_place_by_policy(view, /* allow_cursor */ true, rc.placement_policy);
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, mappable.map);
	if (view->mapped) {
		return;
	}

	/*
	 * An output should have been chosen when the surface was first
	 * created, but take one more opportunity to assign an output if not.
	 */
	if (!view->output) {
		view_set_output(view, output_nearest_to_cursor(view->server));
	}

	view->mapped = true;

	if (!view->been_mapped) {
		if (view_wants_decorations(view)) {
			view_set_ssd_mode(view, LAB_SSD_MODE_FULL);
		} else {
			view_set_ssd_mode(view, LAB_SSD_MODE_NONE);
		}

		/*
		 * Set initial "pending" dimensions. "Current"
		 * dimensions remain zero until handle_commit().
		 */
		if (wlr_box_empty(&view->pending)) {
			struct wlr_xdg_surface *xdg_surface =
				xdg_surface_from_view(view);
			view->pending.width = xdg_surface->geometry.width;
			view->pending.height = xdg_surface->geometry.height;
		}

		/*
		 * Set initial "pending" position for floating views.
		 */
		if (view_is_floating(view)) {
			set_initial_position(view);
		}

		/* Disable background fill at map (paranoid?) */
		disable_fullscreen_bg(view);

		/*
		 * Set initial "current" position directly before
		 * calling view_moved() to reduce flicker
		 */
		view->current.x = view->pending.x;
		view->current.y = view->pending.y;

		view_moved(view);
	}

	view_impl_map(view);
	view->been_mapped = true;
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, mappable.unmap);
	if (view->mapped) {
		view->mapped = false;
		view_impl_unmap(view);
	}
}

static pid_t
xdg_view_get_pid(struct view *view)
{
	assert(view);
	pid_t pid = -1;

	if (view->surface && view->surface->resource
			&& view->surface->resource->client) {
		struct wl_client *client = view->surface->resource->client;
		wl_client_get_credentials(client, &pid, NULL, NULL);
	}
	return pid;
}

static const struct view_impl xdg_toplevel_view_impl = {
	.configure = xdg_toplevel_view_configure,
	.close = xdg_toplevel_view_close,
	.set_activated = xdg_toplevel_view_set_activated,
	.set_fullscreen = xdg_toplevel_view_set_fullscreen,
	.notify_tiled = xdg_toplevel_view_notify_tiled,
	.maximize = xdg_toplevel_view_maximize,
	.minimize = xdg_toplevel_view_minimize,
	.get_parent = xdg_toplevel_view_get_parent,
	.get_root = xdg_toplevel_view_get_root,
	.append_children = xdg_toplevel_view_append_children,
	.is_modal_dialog = xdg_toplevel_view_is_modal_dialog,
	.get_size_hints = xdg_toplevel_view_get_size_hints,
	.contains_window_type = xdg_toplevel_view_contains_window_type,
	.get_pid = xdg_view_get_pid,
};

struct token_data {
	bool had_valid_surface;
	bool had_valid_seat;
	struct wl_listener destroy;
};

static void
handle_xdg_activation_token_destroy(struct wl_listener *listener, void *data)
{
	struct token_data *token_data = wl_container_of(listener, token_data, destroy);
	wl_list_remove(&token_data->destroy.link);
	free(token_data);
}

static void
handle_xdg_activation_new_token(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_activation_token_v1 *token = data;
	struct token_data *token_data = znew(*token_data);
	token_data->had_valid_surface = !!token->surface;
	token_data->had_valid_seat = !!token->seat;
	token->data = token_data;

	token_data->destroy.notify = handle_xdg_activation_token_destroy;
	wl_signal_add(&token->events.destroy, &token_data->destroy);
}

static void
handle_xdg_activation_request(struct wl_listener *listener, void *data)
{
	const struct wlr_xdg_activation_v1_request_activate_event *event = data;
	struct token_data *token_data = event->token->data;
	assert(token_data);

	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_try_from_wlr_surface(event->surface);
	if (!xdg_surface) {
		return;
	}
	struct view *view = xdg_surface->data;

	if (!view) {
		wlr_log(WLR_INFO, "Not activating surface - no view attached to surface");
		return;
	}

	if (!token_data->had_valid_seat) {
		wlr_log(WLR_INFO, "Denying focus request, seat wasn't supplied");
		return;
	}

	/*
	 * TODO: The verification of source surface is temporarily disabled to
	 * allow activation of some clients (e.g. thunderbird). Reland this
	 * check when we implement the configuration for activation policy or
	 * urgency hints.
	 *
	 * if (!token_data->had_valid_surface) {
	 *	wlr_log(WLR_INFO, "Denying focus request, source surface not set");
	 *	return;
	 * }
	 */

	if (window_rules_get_property(view, "ignoreFocusRequest") == LAB_PROP_TRUE) {
		wlr_log(WLR_INFO, "Ignoring focus request due to window rule configuration");
		return;
	}

	wlr_log(WLR_DEBUG, "Activating surface");
	desktop_focus_view(view, /*raise*/ true);
}

/*
 * We use the following struct user_data pointers:
 *   - wlr_xdg_surface->data = view
 *     for the wlr_xdg_toplevel_decoration_v1 implementation
 *   - wlr_surface->data = scene_tree
 *     to help the popups find their parent nodes
 */
static void
handle_new_xdg_toplevel(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;
	struct wlr_xdg_surface *xdg_surface = xdg_toplevel->base;

	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	struct xdg_toplevel_view *xdg_toplevel_view = znew(*xdg_toplevel_view);
	struct view *view = &xdg_toplevel_view->base;

	view->server = server;
	view->type = LAB_XDG_SHELL_VIEW;
	view->impl = &xdg_toplevel_view_impl;
	view_init(view);

	xdg_toplevel_view->xdg_surface = xdg_surface;

	/*
	 * Pick an output for the surface as soon as its created, so that the
	 * client can be notified about any fractional scale before it is given
	 * the chance to configure itself (and possibly pick its dimensions).
	 */
	view_set_output(view, output_nearest_to_cursor(server));
	if (view->output) {
		wlr_fractional_scale_v1_notify_scale(xdg_surface->surface,
			view->output->wlr_output->scale);
	}

	view->workspace = server->workspaces.current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

	struct wlr_scene_tree *tree = wlr_scene_xdg_surface_create(
		view->scene_tree, xdg_surface);
	if (!tree) {
		/* TODO: might need further clean up */
		wl_resource_post_no_memory(xdg_surface->resource);
		free(xdg_toplevel_view);
		return;
	}
	view->content_tree = tree;
	node_descriptor_create(&view->scene_tree->node,
		LAB_NODE_VIEW, view, /*data*/ NULL);

	/*
	 * The xdg_toplevel_decoration and kde_server_decoration protocols
	 * expects clients to use client side decorations unless server side
	 * decorations are negotiated. So we default to client side ones here.
	 *
	 * TODO: We may want to assign the default based on a new rc.xml
	 *       config option like "enforce-server" in the future.
	 */
	view->ssd_preference = LAB_SSD_PREF_CLIENT;

	/*
	 * xdg_toplevel_decoration and kde_server_decoration use this
	 * pointer to connect the view to a decoration object that may
	 * be created in the future.
	 */
	xdg_surface->data = view;

	/*
	 * GTK4 initializes the decorations on the wl_surface before
	 * converting it into a xdg surface. This call takes care of
	 * connecting the view to an existing decoration. If there
	 * is no existing decoration object available for the
	 * wl_surface, this call is a no-op.
	 */
	kde_server_decoration_set_view(view, xdg_surface->surface);

	/* In support of xdg popups and IME popup */
	view->surface = xdg_surface->surface;
	view->surface->data = tree;

	mappable_connect(&view->mappable, xdg_surface->surface,
		handle_map, handle_unmap);

	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	CONNECT_SIGNAL(toplevel, view, destroy);
	CONNECT_SIGNAL(toplevel, view, request_move);
	CONNECT_SIGNAL(toplevel, view, request_resize);
	CONNECT_SIGNAL(toplevel, view, request_minimize);
	CONNECT_SIGNAL(toplevel, view, request_maximize);
	CONNECT_SIGNAL(toplevel, view, request_fullscreen);
	CONNECT_SIGNAL(toplevel, view, set_title);
	CONNECT_SIGNAL(view->surface, view, commit);

	/* Events specific to XDG toplevel views */
	CONNECT_SIGNAL(toplevel, xdg_toplevel_view, set_app_id);
	CONNECT_SIGNAL(toplevel, xdg_toplevel_view, request_show_window_menu);
	CONNECT_SIGNAL(xdg_surface, xdg_toplevel_view, new_popup);

	wl_list_insert(&server->views, &view->link);
	view->creation_id = server->next_view_creation_id++;
}

static void
handle_xdg_toplevel_icon_set_icon(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_icon_manager_v1_set_icon_event *event = data;
	struct wlr_xdg_surface *xdg_surface = event->toplevel->base;
	struct view *view = xdg_surface->data;
	assert(view);

	char *icon_name = NULL;
	struct wl_array buffers;
	wl_array_init(&buffers);

	if (event->icon) {
		icon_name = event->icon->name;

		struct wlr_xdg_toplevel_icon_v1_buffer *icon_buffer;
		wl_list_for_each(icon_buffer, &event->icon->buffers, link) {
			struct lab_data_buffer *buffer =
				buffer_create_from_wlr_buffer(icon_buffer->buffer);
			if (buffer) {
				array_add(&buffers, buffer);
			}
		}
	}

	/* view takes ownership of the buffers */
	view_set_icon(view, icon_name, &buffers);
	wl_array_release(&buffers);
}

void
xdg_shell_init(struct server *server)
{
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display,
		LAB_XDG_SHELL_VERSION);
	if (!server->xdg_shell) {
		wlr_log(WLR_ERROR, "unable to create the XDG shell interface");
		exit(EXIT_FAILURE);
	}

	server->new_xdg_toplevel.notify = handle_new_xdg_toplevel;
	wl_signal_add(&server->xdg_shell->events.new_toplevel, &server->new_xdg_toplevel);

	server->xdg_activation = wlr_xdg_activation_v1_create(server->wl_display);
	if (!server->xdg_activation) {
		wlr_log(WLR_ERROR, "unable to create xdg_activation interface");
		exit(EXIT_FAILURE);
	}

	server->xdg_activation_request.notify = handle_xdg_activation_request;
	wl_signal_add(&server->xdg_activation->events.request_activate,
		&server->xdg_activation_request);

	server->xdg_activation_new_token.notify = handle_xdg_activation_new_token;
	wl_signal_add(&server->xdg_activation->events.new_token,
		&server->xdg_activation_new_token);

	server->xdg_toplevel_icon_manager = wlr_xdg_toplevel_icon_manager_v1_create(
		server->wl_display, 1);
	server->xdg_toplevel_icon_set_icon.notify = handle_xdg_toplevel_icon_set_icon;
	wl_signal_add(&server->xdg_toplevel_icon_manager->events.set_icon,
		&server->xdg_toplevel_icon_set_icon);

	wlr_xdg_wm_dialog_v1_create(server->wl_display, 1);
}

void
xdg_shell_finish(struct server *server)
{
	wl_list_remove(&server->new_xdg_toplevel.link);
	wl_list_remove(&server->xdg_activation_request.link);
	wl_list_remove(&server->xdg_activation_new_token.link);
	wl_list_remove(&server->xdg_toplevel_icon_set_icon.link);
}
