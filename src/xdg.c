// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_fractional_scale_v1.h>

#include "common/macros.h"
#include "common/mem.h"
#include "decorations.h"
#include "labwc.h"
#include "node.h"
#include "snap-constraints.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"
#include "workspaces.h"

#define LAB_XDG_SHELL_VERSION (3)
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

static bool
xdg_toplevel_view_contains_window_type(struct view *view, int32_t window_type)
{
	assert(view);

	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	struct wlr_xdg_toplevel_state *state = &toplevel->current;
	bool is_dialog = (state->min_width != 0 && state->min_height != 0
		&& (state->min_width == state->max_width
		|| state->min_height == state->max_height))
		|| toplevel->parent;

	switch (window_type) {
	case NET_WM_WINDOW_TYPE_NORMAL:
		return !is_dialog;
	case NET_WM_WINDOW_TYPE_DIALOG:
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
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	assert(view->surface);

	struct wlr_box size;
	wlr_xdg_surface_get_geometry(xdg_surface, &size);

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
		struct wlr_box extent;
		wlr_surface_get_extends(xdg_surface->surface, &extent);
		if (extent.width == view->pending.width
				&& extent.height == view->pending.height) {
			wlr_log(WLR_DEBUG, "window geometry for client (%s) "
				"appears to be incorrect - ignoring",
				view_get_string_prop(view, "app_id"));
			size = extent; /* Use surface extent instead */
		}
	}

	struct wlr_box *current = &view->current;
	bool update_required = current->width != size.width
		|| current->height != size.height;

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
			struct wlr_xdg_toplevel *toplevel =
				xdg_toplevel_from_view(view);
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

	const char *app_id = view_get_string_prop(view, "app_id");
	wlr_log(WLR_INFO, "client (%s) did not respond to configure request "
		"in %d ms", app_id, CONFIGURE_TIMEOUT_MS);

	wl_event_source_remove(view->pending_configure_timeout);
	view->pending_configure_serial = 0;
	view->pending_configure_timeout = NULL;

	view_impl_apply_geometry(view,
		view->current.width, view->current.height);

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

	xdg_toplevel_view->xdg_surface->data = NULL;
	xdg_toplevel_view->xdg_surface = NULL;

	/* Remove xdg-shell view specific listeners */
	wl_list_remove(&xdg_toplevel_view->set_app_id.link);
	wl_list_remove(&xdg_toplevel_view->new_popup.link);

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
	if (view == view->server->seat.pressed.view) {
		interactive_begin(view, LAB_INPUT_STATE_MOVE, 0);
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
	if (view == view->server->seat.pressed.view) {
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
	if (!view->mapped && !view->output) {
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
	bool maximized = xdg_toplevel_from_view(view)->requested.maximized;
	view_maximize(view, maximized ? VIEW_AXIS_BOTH : VIEW_AXIS_NONE,
		/*store_natural_geometry*/ true);
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
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_fullscreen);
	if (!view->mapped && !view->output) {
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
	set_fullscreen_from_request(view,
		&xdg_toplevel_from_view(view)->requested);
}

static void
handle_set_title(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, set_title);
	view_update_title(view);
}

static void
handle_set_app_id(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_view *xdg_toplevel_view =
		wl_container_of(listener, xdg_toplevel_view, set_app_id);
	struct view *view = &xdg_toplevel_view->base;
	view_update_app_id(view);
}

static void
xdg_toplevel_view_configure(struct view *view, struct wlr_box geo)
{
	uint32_t serial = 0;
	view_adjust_size(view, &geo.width, &geo.height);

	/*
	 * We do not need to send a configure request unless the size
	 * changed (wayland has no notion of a global position). If the
	 * size is the same (and there is no pending configure request)
	 * then we can just move the view directly.
	 */
	if (geo.width != view->pending.width
			|| geo.height != view->pending.height) {
		serial = wlr_xdg_toplevel_set_size(xdg_toplevel_from_view(view),
			geo.width, geo.height);
	}

	view->pending = geo;
	if (serial > 0) {
		set_pending_configure_serial(view, serial);
	} else if (view->pending_configure_serial == 0) {
		view->current.x = geo.x;
		view->current.y = geo.y;
		view_moved(view);
	}
}

static void
xdg_toplevel_view_close(struct view *view)
{
	wlr_xdg_toplevel_send_close(xdg_toplevel_from_view(view));
}

static void
xdg_toplevel_view_maximize(struct view *view, bool maximized)
{
	wlr_xdg_toplevel_set_maximized(xdg_toplevel_from_view(view), maximized);
}

static void
xdg_toplevel_view_minimize(struct view *view, bool minimized)
{
	/* noop */
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
	struct wlr_xdg_surface *surface = (struct wlr_xdg_surface *)root->base;
	return (struct view *)surface->data;
}

static void
xdg_toplevel_view_append_children(struct view *self, struct wl_array *children)
{
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(self);
	struct view *view;

	wl_list_for_each_reverse(view, &self->server->views, link)
	{
		if (view == self) {
			continue;
		}
		if (view->type != LAB_XDG_SHELL_VIEW) {
			continue;
		}
		if (!view->mapped && !view->minimized) {
			continue;
		}
		if (top_parent_of(view) != toplevel) {
			continue;
		}
		struct view **child = wl_array_add(children, sizeof(*child));
		*child = view;
	}
}

static void
xdg_toplevel_view_set_activated(struct view *view, bool activated)
{
	wlr_xdg_toplevel_set_activated(xdg_toplevel_from_view(view), activated);
}

static void
xdg_toplevel_view_set_fullscreen(struct view *view, bool fullscreen)
{
	wlr_xdg_toplevel_set_fullscreen(xdg_toplevel_from_view(view),
		fullscreen);
}

static void
xdg_toplevel_view_notify_tiled(struct view *view)
{
	/* Take no action if xdg-shell tiling is disabled */
	if (rc.snap_tiling_events_mode == LAB_TILING_EVENTS_NEVER) {
		return;
	}

	enum wlr_edges edge = WLR_EDGE_NONE;

	bool want_edge = rc.snap_tiling_events_mode & LAB_TILING_EVENTS_EDGE;
	bool want_region = rc.snap_tiling_events_mode & LAB_TILING_EVENTS_REGION;

	/*
	 * Edge-snapped view are considered tiled on the snapped edge and those
	 * perpendicular to it.
	 */
	if (want_edge) {
		switch (view->tiled) {
		case VIEW_EDGE_LEFT:
			edge = WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
			break;
		case VIEW_EDGE_RIGHT:
			edge = WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
			break;
		case VIEW_EDGE_UP:
			edge = WLR_EDGE_TOP | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
			break;
		case VIEW_EDGE_DOWN:
			edge = WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
			break;
		default:
			edge = WLR_EDGE_NONE;
		}
	}

	if (want_region && view->tiled_region) {
		/* Region-snapped views are considered tiled on all edges */
		edge = WLR_EDGE_LEFT | WLR_EDGE_RIGHT |
			WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
	}

	wlr_xdg_toplevel_set_tiled(xdg_toplevel_from_view(view), edge);
}

static struct view *
lookup_view_by_xdg_toplevel(struct server *server,
		struct wlr_xdg_toplevel *xdg_toplevel)
{
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->type != LAB_XDG_SHELL_VIEW) {
			continue;
		}
		if (xdg_toplevel_from_view(view) == xdg_toplevel) {
			return view;
		}
	}
	return NULL;
}

static void
set_initial_position(struct view *view)
{
	struct wlr_xdg_toplevel *parent_xdg_toplevel =
		xdg_toplevel_from_view(view)->parent;

	view_constrain_size_to_that_of_usable_area(view);

	if (parent_xdg_toplevel) {
		/* Child views are center-aligned relative to their parents */
		struct view *parent = lookup_view_by_xdg_toplevel(
			view->server, parent_xdg_toplevel);
		assert(parent);
		view_set_output(view, parent->output);
		view_center(view, &parent->pending);
		return;
	}

	/* All other views are placed according to a configured strategy */
	view_place_by_policy(view, /* allow_cursor */ true, rc.placement_policy);
}

static const char *
xdg_toplevel_view_get_string_prop(struct view *view, const char *prop)
{
	struct xdg_toplevel_view *xdg_view = xdg_toplevel_view_from_view(view);
	struct wlr_xdg_toplevel *xdg_toplevel = xdg_view->xdg_surface
		? xdg_view->xdg_surface->toplevel
		: NULL;
	if (!xdg_toplevel) {
		/*
		 * This may happen due to a matchOnce rule when
		 * a view is destroyed while A-Tab is open. See
		 * https://github.com/labwc/labwc/issues/1082#issuecomment-1716137180
		 */
		return "";
	}

	if (!strcmp(prop, "title")) {
		return xdg_toplevel->title;
	}
	if (!strcmp(prop, "app_id")) {
		return xdg_toplevel->app_id;
	}
	return "";
}

static void
init_foreign_toplevel(struct view *view)
{
	foreign_toplevel_handle_create(view);
	struct wlr_xdg_toplevel *toplevel = xdg_toplevel_from_view(view);
	if (!toplevel->parent) {
		return;
	}
	struct wlr_xdg_surface *surface = toplevel->parent->base;
	struct view *parent = surface->data;
	if (!parent->toplevel.handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_parent(view->toplevel.handle, parent->toplevel.handle);
}

static void
xdg_toplevel_view_map(struct view *view)
{
	if (view->mapped) {
		return;
	}

	view->mapped = true;

	/*
	 * An output should have been chosen when the surface was first
	 * created, but take one more opportunity to assign an output if not.
	 */
	if (!view->output) {
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	view->surface = xdg_surface->surface;
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	if (!view->been_mapped) {
		struct wlr_xdg_toplevel_requested *requested =
			&xdg_toplevel_from_view(view)->requested;

		init_foreign_toplevel(view);

		if (view_wants_decorations(view)) {
			view_set_ssd_mode(view, LAB_SSD_MODE_FULL);
		} else {
			view_set_ssd_mode(view, LAB_SSD_MODE_NONE);
		}

		/*
		 * Set initial "pending" dimensions (may be modified by
		 * view_set_fullscreen/view_maximize() below). "Current"
		 * dimensions remain zero until handle_commit().
		 */
		if (wlr_box_empty(&view->pending)) {
			struct wlr_box size;
			wlr_xdg_surface_get_geometry(xdg_surface, &size);
			view->pending.width = size.width;
			view->pending.height = size.height;
		}

		/*
		 * Set initial "pending" position for floating views.
		 * Do this before view_set_fullscreen/view_maximize() so
		 * that the position is saved with the natural geometry.
		 *
		 * FIXME: the natural geometry is not saved if either
		 * handle_request_fullscreen/handle_request_maximize()
		 * is called before map (try "foot --maximized").
		 */
		if (view_is_floating(view)) {
			set_initial_position(view);
		}

		set_fullscreen_from_request(view, requested);
		view_maximize(view, requested->maximized ?
			VIEW_AXIS_BOTH : VIEW_AXIS_NONE,
			/*store_natural_geometry*/ true);

		/*
		 * Set initial "current" position directly before
		 * calling view_moved() to reduce flicker
		 */
		view->current.x = view->pending.x;
		view->current.y = view->pending.y;

		view_moved(view);
	}

	view->commit.notify = handle_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &view->commit);

	view_impl_map(view);
	view->been_mapped = true;
}

static void
xdg_toplevel_view_unmap(struct view *view, bool client_request)
{
	if (view->mapped) {
		view->mapped = false;
		wlr_scene_node_set_enabled(&view->scene_tree->node, false);
		wl_list_remove(&view->commit.link);
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
	.get_string_prop = xdg_toplevel_view_get_string_prop,
	.map = xdg_toplevel_view_map,
	.set_activated = xdg_toplevel_view_set_activated,
	.set_fullscreen = xdg_toplevel_view_set_fullscreen,
	.notify_tiled = xdg_toplevel_view_notify_tiled,
	.unmap = xdg_toplevel_view_unmap,
	.maximize = xdg_toplevel_view_maximize,
	.minimize = xdg_toplevel_view_minimize,
	.move_to_front = view_impl_move_to_front,
	.move_to_back = view_impl_move_to_back,
	.get_root = xdg_toplevel_view_get_root,
	.append_children = xdg_toplevel_view_append_children,
	.contains_window_type = xdg_toplevel_view_contains_window_type,
	.get_pid = xdg_view_get_pid,
};

struct token_data {
	bool had_valid_surface;
	bool had_valid_seat;
	struct wl_listener destroy;
};

static void
xdg_activation_handle_token_destroy(struct wl_listener *listener, void *data)
{
	struct token_data *token_data = wl_container_of(listener, token_data, destroy);
	wl_list_remove(&token_data->destroy.link);
	free(token_data);
}

static void
xdg_activation_handle_new_token(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_activation_token_v1 *token = data;
	struct token_data *token_data = znew(*token_data);
	token_data->had_valid_surface = !!token->surface;
	token_data->had_valid_seat = !!token->seat;
	token->data = token_data;

	token_data->destroy.notify = xdg_activation_handle_token_destroy;
	wl_signal_add(&token->events.destroy, &token_data->destroy);
}

static void
xdg_activation_handle_request(struct wl_listener *listener, void *data)
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

	if (!token_data->had_valid_surface) {
		wlr_log(WLR_INFO, "Denying focus request, source surface not set");
		return;
	}

	if (window_rules_get_property(view, "ignoreFocusRequest") == LAB_PROP_TRUE) {
		wlr_log(WLR_INFO, "Ignoring focus request due to window rule configuration");
		return;
	}

	if (view->server->osd_state.cycle_view) {
		wlr_log(WLR_INFO, "Preventing focus request while in window switcher");
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
xdg_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	/*
	 * We deal with popups in xdg-popup.c and layers.c as they have to be
	 * treated differently
	 */
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	wlr_xdg_surface_ping(xdg_surface);

	struct xdg_toplevel_view *xdg_toplevel_view = znew(*xdg_toplevel_view);
	struct view *view = &xdg_toplevel_view->base;

	view->server = server;
	view->type = LAB_XDG_SHELL_VIEW;
	view->impl = &xdg_toplevel_view_impl;
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

	view->workspace = server->workspace_current;
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
	view->scene_node = &tree->node;
	node_descriptor_create(&view->scene_tree->node,
		LAB_NODE_DESC_VIEW, view);

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
	xdg_surface->surface->data = tree;

	view_connect_map(view, xdg_surface->surface);
	CONNECT_SIGNAL(xdg_surface, view, destroy);

	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	CONNECT_SIGNAL(toplevel, view, request_move);
	CONNECT_SIGNAL(toplevel, view, request_resize);
	CONNECT_SIGNAL(toplevel, view, request_minimize);
	CONNECT_SIGNAL(toplevel, view, request_maximize);
	CONNECT_SIGNAL(toplevel, view, request_fullscreen);
	CONNECT_SIGNAL(toplevel, view, set_title);

	/* Events specific to XDG toplevel views */
	CONNECT_SIGNAL(toplevel, xdg_toplevel_view, set_app_id);
	CONNECT_SIGNAL(xdg_surface, xdg_toplevel_view, new_popup);

	wl_list_insert(&server->views, &view->link);
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
	server->new_xdg_surface.notify = xdg_surface_new;
	wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);

	server->xdg_activation = wlr_xdg_activation_v1_create(server->wl_display);
	if (!server->xdg_activation) {
		wlr_log(WLR_ERROR, "unable to create xdg_activation interface");
		exit(EXIT_FAILURE);
	}

	server->xdg_activation_request.notify = xdg_activation_handle_request;
	wl_signal_add(&server->xdg_activation->events.request_activate,
		&server->xdg_activation_request);

	server->xdg_activation_new_token.notify = xdg_activation_handle_new_token;
	wl_signal_add(&server->xdg_activation->events.new_token,
		&server->xdg_activation_new_token);
}

