// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "xwayland.h"
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/xwayland.h>
#include "buffer.h"
#include "common/array.h"
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "config/session.h"
#include "foreign-toplevel/foreign.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"
#include "workspaces.h"

enum atoms {
	ATOM_NET_WM_ICON = 0,

	ATOM_COUNT,
};

static const char * const atom_names[] = {
	[ATOM_NET_WM_ICON] = "_NET_WM_ICON",
};

static_assert(ARRAY_SIZE(atom_names) == ATOM_COUNT, "atom names out of sync");

static xcb_atom_t atoms[ATOM_COUNT] = {0};

static void set_surface(struct view *view, struct wlr_surface *surface);
static void handle_map(struct wl_listener *listener, void *data);
static void handle_unmap(struct wl_listener *listener, void *data);

static struct xwayland_view *
xwayland_view_from_view(struct view *view)
{
	assert(view->type == LAB_XWAYLAND_VIEW);
	return (struct xwayland_view *)view;
}

static struct wlr_xwayland_surface *
xwayland_surface_from_view(struct view *view)
{
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	assert(xwayland_view->xwayland_surface);
	return xwayland_view->xwayland_surface;
}

static bool
xwayland_view_contains_window_type(struct view *view,
		enum lab_window_type window_type)
{
	/* Compile-time check that the enum types match */
	static_assert(LAB_WINDOW_TYPE_DESKTOP ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DESKTOP
		&& LAB_WINDOW_TYPE_DOCK ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DOCK
		&& LAB_WINDOW_TYPE_TOOLBAR ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLBAR
		&& LAB_WINDOW_TYPE_MENU ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_MENU
		&& LAB_WINDOW_TYPE_UTILITY ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_UTILITY
		&& LAB_WINDOW_TYPE_SPLASH ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH
		&& LAB_WINDOW_TYPE_DIALOG ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DIALOG
		&& LAB_WINDOW_TYPE_DROPDOWN_MENU ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DROPDOWN_MENU
		&& LAB_WINDOW_TYPE_POPUP_MENU ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_POPUP_MENU
		&& LAB_WINDOW_TYPE_TOOLTIP ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLTIP
		&& LAB_WINDOW_TYPE_NOTIFICATION ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NOTIFICATION
		&& LAB_WINDOW_TYPE_COMBO ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_COMBO
		&& LAB_WINDOW_TYPE_DND ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DND
		&& LAB_WINDOW_TYPE_NORMAL ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NORMAL
		&& LAB_WINDOW_TYPE_LEN ==
			(int)WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NORMAL + 1,
		"lab_window_type does not match wlr_xwayland_net_wm_window_type");

	assert(view);
	struct wlr_xwayland_surface *surface = xwayland_surface_from_view(view);
	return wlr_xwayland_surface_has_window_type(surface,
		(enum wlr_xwayland_net_wm_window_type)window_type);
}

static struct view_size_hints
xwayland_view_get_size_hints(struct view *view)
{
	xcb_size_hints_t *hints = xwayland_surface_from_view(view)->size_hints;
	if (!hints) {
		return (struct view_size_hints){0};
	}
	return (struct view_size_hints){
		.min_width = hints->min_width,
		.min_height = hints->min_height,
		.width_inc = hints->width_inc,
		.height_inc = hints->height_inc,
		.base_width = hints->base_width,
		.base_height = hints->base_height,
	};
}

static enum view_wants_focus
xwayland_view_wants_focus(struct view *view)
{
	struct wlr_xwayland_surface *xsurface =
		xwayland_surface_from_view(view);

	switch (wlr_xwayland_surface_icccm_input_model(xsurface)) {
	/*
	 * Abbreviated from ICCCM section 4.1.7 (Input Focus):
	 *
	 * Passive Input - The client expects keyboard input but never
	 * explicitly sets the input focus.
	 * Locally Active Input - The client expects keyboard input and
	 * explicitly sets the input focus, but it only does so when one
	 * of its windows already has the focus.
	 *
	 * Passive and Locally Active clients set the input field of
	 * WM_HINTS to True, which indicates that they require window
	 * manager assistance in acquiring the input focus.
	 */
	case WLR_ICCCM_INPUT_MODEL_PASSIVE:
	case WLR_ICCCM_INPUT_MODEL_LOCAL:
		return VIEW_WANTS_FOCUS_ALWAYS;

	/*
	 * Globally Active Input - The client expects keyboard input and
	 * explicitly sets the input focus, even when it is in windows
	 * the client does not own. ... It wants to prevent the window
	 * manager from setting the input focus to any of its windows
	 * [because it may or may not want focus].
	 *
	 * Globally Active client windows may receive a WM_TAKE_FOCUS
	 * message from the window manager. If they want the focus, they
	 * should respond with a SetInputFocus request.
	 */
	case WLR_ICCCM_INPUT_MODEL_GLOBAL:
		/*
		 * Assume that NORMAL and DIALOG windows are likely to
		 * want focus. These window types should show up in the
		 * Alt-Tab switcher and be automatically focused when
		 * they become topmost.
		 */
		return (wlr_xwayland_surface_has_window_type(xsurface,
				WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NORMAL)
			|| wlr_xwayland_surface_has_window_type(xsurface,
				WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DIALOG)) ?
			VIEW_WANTS_FOCUS_LIKELY : VIEW_WANTS_FOCUS_UNLIKELY;

	/*
	 * No Input - The client never expects keyboard input.
	 *
	 * No Input and Globally Active clients set the input field to
	 * False, which requests that the window manager not set the
	 * input focus to their top-level window.
	 */
	case WLR_ICCCM_INPUT_MODEL_NONE:
		break;
	}

	return VIEW_WANTS_FOCUS_NEVER;
}

static bool
xwayland_view_has_strut_partial(struct view *view)
{
	struct wlr_xwayland_surface *xsurface =
		xwayland_surface_from_view(view);
	return (bool)xsurface->strut_partial;
}

static void
xwayland_view_offer_focus(struct view *view)
{
	wlr_xwayland_surface_offer_focus(xwayland_surface_from_view(view));
}

static struct view *
xwayland_view_get_parent(struct view *view)
{
	struct wlr_xwayland_surface *xsurface = xwayland_surface_from_view(view);
	return xsurface->parent ? (struct view *)xsurface->parent->data : NULL;
}

static struct wlr_xwayland_surface *
top_parent_of(struct view *view)
{
	struct wlr_xwayland_surface *s = xwayland_surface_from_view(view);
	while (s->parent) {
		s = s->parent;
	}
	return s;
}

static void
ensure_initial_geometry_and_output(struct view *view)
{
	if (wlr_box_empty(&view->current)) {
		struct wlr_xwayland_surface *xwayland_surface =
			xwayland_surface_from_view(view);
		view->current.x = xwayland_surface->x;
		view->current.y = xwayland_surface->y;
		view->current.width = xwayland_surface->width;
		view->current.height = xwayland_surface->height;
		/*
		 * If there is no pending move/resize yet, then set
		 * current values (used in map()).
		 */
		if (wlr_box_empty(&view->pending)) {
			view->pending = view->current;
		}
	}
	if (!view->output) {
		/*
		 * Just use the cursor output since we don't know yet
		 * whether the surface position is meaningful.
		 */
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
}

static bool
want_deco(struct wlr_xwayland_surface *xwayland_surface)
{
	struct view *view = (struct view *)xwayland_surface->data;

	/* Window-rules take priority if they exist for this view */
	switch (window_rules_get_property(view, "serverDecoration")) {
	case LAB_PROP_TRUE:
		return true;
	case LAB_PROP_FALSE:
		return false;
	default:
		break;
	}

	return xwayland_surface->decorations ==
		WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	assert(data && data == view->surface);

	/* Must receive commit signal before accessing surface->current* */
	struct wlr_surface_state *state = &view->surface->current;
	struct wlr_box *current = &view->current;

	/*
	 * If there is a pending move/resize, wait until the surface
	 * size changes to update geometry. The hope is to update both
	 * the position and the size of the view at the same time,
	 * reducing visual glitches.
	 */
	if (current->width != state->width || current->height != state->height) {
		view_impl_apply_geometry(view, state->width, state->height);
		view_moved(view);
	}
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
	struct wlr_xwayland_resize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_resize);
	if (view == view->server->seat.pressed.ctx.view) {
		interactive_begin(view, LAB_INPUT_STATE_RESIZE, event->edges);
	}
}

static void
handle_associate(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, associate);
	assert(xwayland_view->xwayland_surface &&
		xwayland_view->xwayland_surface->surface);

	set_surface(&xwayland_view->base,
		xwayland_view->xwayland_surface->surface);
}

static void
handle_dissociate(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, dissociate);

	set_surface(&xwayland_view->base, NULL);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	assert(xwayland_view->xwayland_surface->data == view);

	set_surface(view, NULL);

	/*
	 * Break view <-> xsurface association.  Note that the xsurface
	 * may not actually be destroyed at this point; it may become an
	 * "unmanaged" surface instead (in that case it is important
	 * that xsurface->data not point to the destroyed view).
	 */
	xwayland_view->xwayland_surface->data = NULL;
	xwayland_view->xwayland_surface = NULL;

	/* Remove XWayland view specific listeners */
	wl_list_remove(&xwayland_view->associate.link);
	wl_list_remove(&xwayland_view->dissociate.link);
	wl_list_remove(&xwayland_view->request_activate.link);
	wl_list_remove(&xwayland_view->request_configure.link);
	wl_list_remove(&xwayland_view->set_class.link);
	wl_list_remove(&xwayland_view->set_decorations.link);
	wl_list_remove(&xwayland_view->set_override_redirect.link);
	wl_list_remove(&xwayland_view->set_strut_partial.link);
	wl_list_remove(&xwayland_view->set_window_type.link);
	wl_list_remove(&xwayland_view->focus_in.link);
	wl_list_remove(&xwayland_view->map_request.link);

	view_destroy(view);
}

static void
xwayland_view_configure(struct view *view, struct wlr_box geo)
{
	view->pending = geo;
	wlr_xwayland_surface_configure(xwayland_surface_from_view(view),
		geo.x, geo.y, geo.width, geo.height);

	/*
	 * For unknown reasons, XWayland surfaces that are completely
	 * offscreen seem not to generate commit events. In rare cases,
	 * this can prevent an offscreen window from moving onscreen
	 * (since we wait for a commit event that never occurs). As a
	 * workaround, move offscreen surfaces immediately.
	 */
	bool is_offscreen = !wlr_box_empty(&view->current) &&
		!wlr_output_layout_intersects(view->server->output_layout, NULL,
			&view->current);

	/* If not resizing, process the move immediately */
	if (is_offscreen || (view->current.width == geo.width
			&& view->current.height == geo.height)) {
		view->current.x = geo.x;
		view->current.y = geo.y;
		view_moved(view);
	}
}

static void
handle_request_configure(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_configure);
	struct view *view = &xwayland_view->base;
	struct wlr_xwayland_surface_configure_event *event = data;
	bool ignore_configure_requests = window_rules_get_property(
		view, "ignoreConfigureRequest") == LAB_PROP_TRUE;

	if (view_is_floating(view) && !ignore_configure_requests) {
		/* Honor client configure requests for floating views */
		struct wlr_box box = {.x = event->x, .y = event->y,
			.width = event->width, .height = event->height};
		view_adjust_size(view, &box.width, &box.height);
		xwayland_view_configure(view, box);
	} else {
		/*
		 * Do not allow clients to request geometry other than
		 * what we computed for maximized/fullscreen/tiled
		 * views. Ignore the client request and send back a
		 * ConfigureNotify event with the computed geometry.
		 */
		xwayland_view_configure(view, view->pending);
	}
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_activate);
	struct view *view = &xwayland_view->base;

	if (window_rules_get_property(view, "ignoreFocusRequest") == LAB_PROP_TRUE) {
		wlr_log(WLR_INFO, "Ignoring focus request due to window rule configuration");
		return;
	}

	desktop_focus_view(view, /*raise*/ true);
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_minimize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_minimize);
	view_minimize(view, event->minimize);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_maximize);
	struct wlr_xwayland_surface *surf = xwayland_surface_from_view(view);
	if (!view->mapped) {
		ensure_initial_geometry_and_output(view);
		/*
		 * Set decorations early to avoid changing geometry
		 * after maximize (reduces visual glitches).
		 */
		if (want_deco(surf)) {
			view_set_ssd_mode(view, LAB_SSD_MODE_FULL);
		} else {
			view_set_ssd_mode(view, LAB_SSD_MODE_NONE);
		}
	}

	enum view_axis maximize = VIEW_AXIS_NONE;
	if (surf->maximized_vert) {
		maximize |= VIEW_AXIS_VERTICAL;
	}
	if (surf->maximized_horz) {
		maximize |= VIEW_AXIS_HORIZONTAL;
	}
	view_maximize(view, maximize, /*store_natural_geometry*/ true);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_fullscreen);
	bool fullscreen = xwayland_surface_from_view(view)->fullscreen;
	if (!view->mapped) {
		ensure_initial_geometry_and_output(view);
	}
	view_set_fullscreen(view, fullscreen);
}

static void
handle_set_title(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, set_title);
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	view_set_title(view, xwayland_view->xwayland_surface->title);
}

static void
handle_set_class(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_class);
	struct view *view = &xwayland_view->base;

	/*
	 * Use the WM_CLASS 'instance' (1st string) for the app_id. Per
	 * ICCCM, this is usually "the trailing part of the name used to
	 * invoke the program (argv[0] stripped of any directory names)".
	 *
	 * In most cases, the 'class' (2nd string) is the same as the
	 * 'instance' except for being capitalized. We want lowercase
	 * here since we use the app_id for icon lookups.
	 */
	view_set_app_id(view, xwayland_view->xwayland_surface->instance);
}

static void
xwayland_view_close(struct view *view)
{
	wlr_xwayland_surface_close(xwayland_surface_from_view(view));
}

static void
handle_set_decorations(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_decorations);
	struct view *view = &xwayland_view->base;

	if (want_deco(xwayland_view->xwayland_surface)) {
		view_set_ssd_mode(view, LAB_SSD_MODE_FULL);
	} else {
		view_set_ssd_mode(view, LAB_SSD_MODE_NONE);
	}
}

static void
handle_set_window_type(struct wl_listener *listener, void *data)
{
	/* Intentionally left blank */
}

static void
handle_set_override_redirect(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_override_redirect);
	struct view *view = &xwayland_view->base;
	struct wlr_xwayland_surface *xsurface = xwayland_view->xwayland_surface;

	struct server *server = view->server;
	bool mapped = xsurface->surface && xsurface->surface->mapped;
	if (mapped) {
		handle_unmap(&view->mappable.unmap, NULL);
	}
	handle_destroy(&view->destroy, xsurface);
	/* view is invalid after this point */
	xwayland_unmanaged_create(server, xsurface, mapped);
}

static void
handle_set_strut_partial(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_strut_partial);
	struct view *view = &xwayland_view->base;

	if (view->mapped) {
		output_update_all_usable_areas(view->server, false);
	}
}

static void
update_icon(struct xwayland_view *xwayland_view)
{
	if (!xwayland_view->xwayland_surface) {
		return;
	}

	xcb_window_t window_id = xwayland_view->xwayland_surface->window_id;

	xcb_connection_t *xcb_conn = wlr_xwayland_get_xwm_connection(
		xwayland_view->base.server->xwayland);
	xcb_get_property_cookie_t cookie = xcb_get_property(xcb_conn, 0,
		window_id, atoms[ATOM_NET_WM_ICON], XCB_ATOM_CARDINAL, 0, 0x10000);
	xcb_get_property_reply_t *reply = xcb_get_property_reply(xcb_conn, cookie, NULL);
	if (!reply) {
		return;
	}
	xcb_ewmh_get_wm_icon_reply_t icon;
	if (!xcb_ewmh_get_wm_icon_from_reply(&icon, reply)) {
		wlr_log(WLR_INFO, "Invalid x11 icon");
		view_set_icon(&xwayland_view->base, NULL, NULL);
		goto out;
	}

	xcb_ewmh_wm_icon_iterator_t iter = xcb_ewmh_get_wm_icon_iterator(&icon);
	struct wl_array buffers;
	wl_array_init(&buffers);
	for (; iter.rem; xcb_ewmh_get_wm_icon_next(&iter)) {
		size_t stride = iter.width * 4;
		uint32_t *buf = xzalloc(iter.height * stride);

		/* Pre-multiply alpha */
		for (uint32_t y = 0; y < iter.height; y++) {
			for (uint32_t x = 0; x < iter.width; x++) {
				uint32_t i = x + y * iter.width;
				uint8_t *src_pixel = (uint8_t *)&iter.data[i];
				uint8_t *dst_pixel = (uint8_t *)&buf[i];
				dst_pixel[0] = src_pixel[0] * src_pixel[3] / 255;
				dst_pixel[1] = src_pixel[1] * src_pixel[3] / 255;
				dst_pixel[2] = src_pixel[2] * src_pixel[3] / 255;
				dst_pixel[3] = src_pixel[3];
			}
		}

		struct lab_data_buffer *buffer = buffer_create_from_data(
			buf, iter.width, iter.height, stride);
		array_add(&buffers, buffer);
	}

	/* view takes ownership of the buffers */
	view_set_icon(&xwayland_view->base, NULL, &buffers);
	wl_array_release(&buffers);

out:
	free(reply);
}

static void
handle_focus_in(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, focus_in);
	struct view *view = &xwayland_view->base;
	struct seat *seat = &view->server->seat;

	if (!view->surface) {
		/*
		 * It is rare but possible for the focus_in event to be
		 * received before the map event. This has been seen
		 * during CLion startup, when focus is initially offered
		 * to the splash screen but accepted later by the main
		 * window instead. (In this case, the focus transfer is
		 * client-initiated but allowed by wlroots because the
		 * same PID owns both windows.)
		 *
		 * Set a flag to record this condition, and update the
		 * seat focus later when the view is actually mapped.
		 */
		wlr_log(WLR_DEBUG, "focus_in received before map");
		xwayland_view->focused_before_map = true;
		return;
	}

	if (view->surface != seat->seat->keyboard_state.focused_surface) {
		seat_focus_surface(seat, view->surface);
	}
}

/*
 * Sets the initial geometry of maximized/fullscreen views before
 * actually mapping them, so that they can do their initial layout and
 * drawing with the correct geometry. This avoids visual glitches and
 * also avoids undesired layout changes with some apps (e.g. HomeBank).
 */
static void
handle_map_request(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, map_request);
	struct view *view = &xwayland_view->base;
	struct wlr_xwayland_surface *xsurface = xwayland_view->xwayland_surface;

	if (view->mapped) {
		/* Probably shouldn't happen, but be sure */
		return;
	}

	/* Keep the view invisible until actually mapped */
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	ensure_initial_geometry_and_output(view);

	/*
	 * Per the Extended Window Manager Hints (EWMH) spec: "The Window
	 * Manager SHOULD honor _NET_WM_STATE whenever a withdrawn window
	 * requests to be mapped."
	 *
	 * The following order of operations is intended to reduce the
	 * number of resize (Configure) events:
	 *   1. set fullscreen state
	 *   2. set decorations (depends on fullscreen state)
	 *   3. set maximized (geometry depends on decorations)
	 */
	view_set_fullscreen(view, xsurface->fullscreen);
	if (!view->been_mapped) {
		if (want_deco(xsurface)) {
			view_set_ssd_mode(view, LAB_SSD_MODE_FULL);
		} else {
			view_set_ssd_mode(view, LAB_SSD_MODE_NONE);
		}
	}
	enum view_axis axis = VIEW_AXIS_NONE;
	if (xsurface->maximized_horz) {
		axis |= VIEW_AXIS_HORIZONTAL;
	}
	if (xsurface->maximized_vert) {
		axis |= VIEW_AXIS_VERTICAL;
	}
	view_maximize(view, axis, /*store_natural_geometry*/ true);
	/*
	 * We could also call set_initial_position() here, but it's not
	 * really necessary until the view is actually mapped (and at
	 * that point the output layout is known for sure).
	 */
}

static void
check_natural_geometry(struct view *view)
{
	/*
	 * Some applications (example: Thonny) don't set a reasonable
	 * un-maximized size when started maximized. Try to detect this
	 * and set a fallback size.
	 */
	if (!view_is_floating(view)
			&& (view->natural_geometry.width < LAB_MIN_VIEW_WIDTH
			|| view->natural_geometry.height < LAB_MIN_VIEW_HEIGHT)) {
		view->natural_geometry = view_get_fallback_natural_geometry(view);
	}
}

static void
set_initial_position(struct view *view,
		struct wlr_xwayland_surface *xwayland_surface)
{
	/* Don't center views with position explicitly specified */
	bool has_position = xwayland_surface->size_hints &&
		(xwayland_surface->size_hints->flags & (
			XCB_ICCCM_SIZE_HINT_US_POSITION |
			XCB_ICCCM_SIZE_HINT_P_POSITION));

	if (!has_position) {
		view_constrain_size_to_that_of_usable_area(view);

		if (view_is_floating(view)) {
			view_place_by_policy(view,
				/* allow_cursor */ true, rc.placement_policy);
		} else {
			/*
			 * View is maximized/fullscreen. Center the
			 * stored natural geometry without actually
			 * moving the view.
			 */
			view_compute_centered_position(view, NULL,
				view->natural_geometry.width,
				view->natural_geometry.height,
				&view->natural_geometry.x,
				&view->natural_geometry.y);
		}
	}

	/*
	 * Always make sure the view is onscreen and adjusted for any
	 * layout changes that could have occurred between map_request
	 * and the actual map event.
	 */
	view_adjust_for_layout_change(view);
}

static void
set_surface(struct view *view, struct wlr_surface *surface)
{
	if (view->surface) {
		/* Disconnect wlr_surface event listeners */
		mappable_disconnect(&view->mappable);
		wl_list_remove(&view->commit.link);
	}
	view->surface = surface;
	if (surface) {
		/* Connect wlr_surface event listeners */
		mappable_connect(&view->mappable, surface,
			handle_map, handle_unmap);
		CONNECT_SIGNAL(surface, view, commit);
	}
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, mappable.map);
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	struct wlr_xwayland_surface *xwayland_surface =
		xwayland_view->xwayland_surface;
	assert(xwayland_surface);
	assert(xwayland_surface->surface);
	assert(xwayland_surface->surface == view->surface);

	if (view->mapped) {
		return;
	}

	/*
	 * The map_request event may not be received when an unmanaged
	 * (override-redirect) surface becomes managed. To make sure we
	 * have valid geometry in that case, call handle_map_request()
	 * explicitly (calling it twice is harmless).
	 */
	handle_map_request(&xwayland_view->map_request, NULL);

	view->mapped = true;

	if (!view->content_tree) {
		view->content_tree = wlr_scene_subsurface_tree_create(
			view->scene_tree, view->surface);
		if (!view->content_tree) {
			/* TODO: might need further clean up */
			wl_resource_post_no_memory(view->surface->resource);
			return;
		}
	}

	wlr_scene_node_set_enabled(&view->content_tree->node, !view->shaded);

	if (!view->been_mapped) {
		check_natural_geometry(view);
		set_initial_position(view, xwayland_surface);
		/*
		 * When mapping the view for the first time, visual
		 * artifacts are reduced if we display it immediately at
		 * the final intended position/size rather than waiting
		 * for handle_commit().
		 */
		view->current = view->pending;
		view_moved(view);
	}

	/*
	 * If the view was focused (on the xwayland server side) before
	 * being mapped, update the seat focus now. Note that this only
	 * really matters in the case of Globally Active input windows.
	 * In all other cases, it's redundant since view_impl_map()
	 * results in the view being focused anyway.
	 */
	if (xwayland_view->focused_before_map) {
		xwayland_view->focused_before_map = false;
		seat_focus_surface(&view->server->seat, view->surface);
	}

	view_impl_map(view);
	view->been_mapped = true;
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, mappable.unmap);
	if (!view->mapped) {
		return;
	}
	view->mapped = false;
	view_impl_unmap(view);

	/*
	 * Destroy the content_tree at unmap. Alternatively, we could
	 * let wlr_scene manage its lifetime automatically, but this
	 * approach is symmetrical with handle_map() and avoids any
	 * concern of a dangling pointer in view->content_tree.
	 */
	if (view->content_tree) {
		wlr_scene_node_destroy(&view->content_tree->node);
		view->content_tree = NULL;
	}
}

static void
xwayland_view_maximize(struct view *view, enum view_axis maximized)
{
	wlr_xwayland_surface_set_maximized(xwayland_surface_from_view(view),
		maximized & VIEW_AXIS_HORIZONTAL, maximized & VIEW_AXIS_VERTICAL);
}

static void
xwayland_view_minimize(struct view *view, bool minimized)
{
	wlr_xwayland_surface_set_minimized(xwayland_surface_from_view(view),
		minimized);
}

static struct view *
xwayland_view_get_root(struct view *view)
{
	struct wlr_xwayland_surface *root = top_parent_of(view);

	/*
	 * The case of root->data == NULL is unlikely, but has been reported
	 * when starting XWayland games (for example 'Fall Guys'). It is
	 * believed to be caused by setting override-redirect on the root
	 * wlr_xwayland_surface making it not be associated with a view anymore.
	 */
	return (root && root->data) ? (struct view *)root->data : view;
}

static void
xwayland_view_append_children(struct view *self, struct wl_array *children)
{
	struct wlr_xwayland_surface *surface = xwayland_surface_from_view(self);
	struct view *view;

	wl_list_for_each_reverse(view, &self->server->views, link) {
		if (view == self) {
			continue;
		}
		if (view->type != LAB_XWAYLAND_VIEW) {
			continue;
		}
		/*
		 * This happens when a view has never been mapped or when a
		 * client has requested a `handle_unmap`.
		 */
		if (!view->surface) {
			continue;
		}
		if (!view->mapped) {
			continue;
		}
		if (top_parent_of(view) != surface) {
			continue;
		}
		struct view **child = wl_array_add(children, sizeof(*child));
		*child = view;
	}
}

static bool
xwayland_view_is_modal_dialog(struct view *self)
{
	return xwayland_surface_from_view(self)->modal;
}

static void
xwayland_view_set_activated(struct view *view, bool activated)
{
	struct wlr_xwayland_surface *xwayland_surface =
		xwayland_surface_from_view(view);

	if (activated && xwayland_surface->minimized) {
		wlr_xwayland_surface_set_minimized(xwayland_surface, false);
	}

	wlr_xwayland_surface_activate(xwayland_surface, activated);
}

static void
xwayland_view_set_fullscreen(struct view *view, bool fullscreen)
{
	wlr_xwayland_surface_set_fullscreen(xwayland_surface_from_view(view),
		fullscreen);
}

static pid_t
xwayland_view_get_pid(struct view *view)
{
	assert(view);

	struct wlr_xwayland_surface *xwayland_surface =
		xwayland_surface_from_view(view);
	if (!xwayland_surface) {
		return -1;
	}
	return xwayland_surface->pid;
}

static const struct view_impl xwayland_view_impl = {
	.configure = xwayland_view_configure,
	.close = xwayland_view_close,
	.set_activated = xwayland_view_set_activated,
	.set_fullscreen = xwayland_view_set_fullscreen,
	.maximize = xwayland_view_maximize,
	.minimize = xwayland_view_minimize,
	.get_parent = xwayland_view_get_parent,
	.get_root = xwayland_view_get_root,
	.append_children = xwayland_view_append_children,
	.is_modal_dialog = xwayland_view_is_modal_dialog,
	.get_size_hints = xwayland_view_get_size_hints,
	.wants_focus = xwayland_view_wants_focus,
	.offer_focus = xwayland_view_offer_focus,
	.has_strut_partial = xwayland_view_has_strut_partial,
	.contains_window_type = xwayland_view_contains_window_type,
	.get_pid = xwayland_view_get_pid,
};

void
xwayland_view_create(struct server *server,
		struct wlr_xwayland_surface *xsurface, bool mapped)
{
	struct xwayland_view *xwayland_view = znew(*xwayland_view);
	struct view *view = &xwayland_view->base;

	view->server = server;
	view->type = LAB_XWAYLAND_VIEW;
	view->impl = &xwayland_view_impl;
	view_init(view);

	/*
	 * Set two-way view <-> xsurface association.  Usually the association
	 * remains until the xsurface is destroyed (which also destroys the
	 * view).  The only exception is caused by setting override-redirect on
	 * the xsurface, which removes it from the view (destroying the view)
	 * and makes it an "unmanaged" surface.
	 */
	xwayland_view->xwayland_surface = xsurface;
	xsurface->data = view;

	view->workspace = server->workspaces.current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	node_descriptor_create(&view->scene_tree->node,
		LAB_NODE_VIEW, view, /*data*/ NULL);

	CONNECT_SIGNAL(xsurface, view, destroy);
	CONNECT_SIGNAL(xsurface, view, request_minimize);
	CONNECT_SIGNAL(xsurface, view, request_maximize);
	CONNECT_SIGNAL(xsurface, view, request_fullscreen);
	CONNECT_SIGNAL(xsurface, view, request_move);
	CONNECT_SIGNAL(xsurface, view, request_resize);
	CONNECT_SIGNAL(xsurface, view, set_title);

	/* Events specific to XWayland views */
	CONNECT_SIGNAL(xsurface, xwayland_view, associate);
	CONNECT_SIGNAL(xsurface, xwayland_view, dissociate);
	CONNECT_SIGNAL(xsurface, xwayland_view, request_activate);
	CONNECT_SIGNAL(xsurface, xwayland_view, request_configure);
	CONNECT_SIGNAL(xsurface, xwayland_view, set_class);
	CONNECT_SIGNAL(xsurface, xwayland_view, set_decorations);
	CONNECT_SIGNAL(xsurface, xwayland_view, set_override_redirect);
	CONNECT_SIGNAL(xsurface, xwayland_view, set_strut_partial);
	CONNECT_SIGNAL(xsurface, xwayland_view, set_window_type);
	CONNECT_SIGNAL(xsurface, xwayland_view, focus_in);
	CONNECT_SIGNAL(xsurface, xwayland_view, map_request);

	wl_list_insert(&view->server->views, &view->link);
	view->creation_id = view->server->next_view_creation_id++;

	if (xsurface->surface) {
		handle_associate(&xwayland_view->associate, NULL);
	}
	if (mapped) {
		handle_map(&xwayland_view->base.mappable.map, NULL);
	}
}

static void
handle_new_surface(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, xwayland_new_surface);
	struct wlr_xwayland_surface *xsurface = data;

	/*
	 * We do not create 'views' for xwayland override_redirect surfaces,
	 * but add them to server.unmanaged_surfaces so that we can render them
	 */
	if (xsurface->override_redirect) {
		xwayland_unmanaged_create(server, xsurface, /* mapped */ false);
	} else {
		xwayland_view_create(server, xsurface, /* mapped */ false);
	}
}

static struct xwayland_view *
xwayland_view_from_window_id(struct server *server, xcb_window_t id)
{
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->type != LAB_XWAYLAND_VIEW) {
			continue;
		}
		struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
		if (xwayland_view->xwayland_surface
				&& xwayland_view->xwayland_surface->window_id == id) {
			return xwayland_view;
		}
	}
	return NULL;
}

#define XCB_EVENT_RESPONSE_TYPE_MASK 0x7f
static bool
handle_x11_event(struct wlr_xwayland *wlr_xwayland, xcb_generic_event_t *event)
{
	switch (event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
	case XCB_PROPERTY_NOTIFY: {
		xcb_property_notify_event_t *ev = (void *)event;
		if (ev->atom == atoms[ATOM_NET_WM_ICON]) {
			struct server *server = wlr_xwayland->data;
			struct xwayland_view *xwayland_view =
				xwayland_view_from_window_id(server, ev->window);
			if (xwayland_view) {
				update_icon(xwayland_view);
			} else {
				wlr_log(WLR_DEBUG, "icon property changed for unknown window");
			}
			return true;
		}
		break;
	}
	default:
		break;
	}

	return false;
}

static void
sync_atoms(struct server *server)
{
	xcb_connection_t *xcb_conn =
		wlr_xwayland_get_xwm_connection(server->xwayland);
	assert(xcb_conn);

	wlr_log(WLR_DEBUG, "Syncing X11 atoms");
	xcb_intern_atom_cookie_t cookies[ATOM_COUNT];

	/* First request everything and then loop over the results to reduce latency */
	for (size_t i = 0; i < ATOM_COUNT; i++) {
		cookies[i] = xcb_intern_atom(xcb_conn, 0,
			strlen(atom_names[i]), atom_names[i]);
	}

	for (size_t i = 0; i < ATOM_COUNT; i++) {
		xcb_generic_error_t *err = NULL;
		xcb_intern_atom_reply_t *reply =
			xcb_intern_atom_reply(xcb_conn, cookies[i], &err);
		if (reply) {
			atoms[i] = reply->atom;
			wlr_log(WLR_DEBUG, "Got X11 atom for %s: %u",
				atom_names[i], reply->atom);
		}
		if (err) {
			atoms[i] = XCB_ATOM_NONE;
			wlr_log(WLR_INFO, "Failed to get X11 atom for %s",
				atom_names[i]);
		}
		free(reply);
		free(err);
	}
}

static void
handle_server_ready(struct wl_listener *listener, void *data)
{
	/* Fire an Xwayland startup script if one (or many) can be found */
	session_run_script("xinitrc");

	struct server *server =
		wl_container_of(listener, server, xwayland_server_ready);
	sync_atoms(server);
}

static void
handle_xwm_ready(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, xwayland_xwm_ready);
	wlr_xwayland_set_seat(server->xwayland, server->seat.seat);
	xwayland_update_workarea(server);
}

void
xwayland_server_init(struct server *server, struct wlr_compositor *compositor)
{
	server->xwayland =
		wlr_xwayland_create(server->wl_display,
			compositor, /* lazy */ !rc.xwayland_persistence);
	if (!server->xwayland) {
		wlr_log(WLR_ERROR, "cannot create xwayland server");
		exit(EXIT_FAILURE);
	}
	server->xwayland_new_surface.notify = handle_new_surface;
	wl_signal_add(&server->xwayland->events.new_surface,
		&server->xwayland_new_surface);

	server->xwayland_server_ready.notify = handle_server_ready;
	wl_signal_add(&server->xwayland->server->events.ready,
		&server->xwayland_server_ready);

	server->xwayland_xwm_ready.notify = handle_xwm_ready;
	wl_signal_add(&server->xwayland->events.ready,
		&server->xwayland_xwm_ready);

	server->xwayland->data = server;
	server->xwayland->user_event_handler = handle_x11_event;

	if (setenv("DISPLAY", server->xwayland->display_name, true) < 0) {
		wlr_log_errno(WLR_ERROR, "unable to set DISPLAY for xwayland");
	} else {
		wlr_log(WLR_DEBUG, "xwayland is running on display %s",
			server->xwayland->display_name);
	}

	struct wlr_xcursor *xcursor;
	xcursor = wlr_xcursor_manager_get_xcursor(
		server->seat.xcursor_manager, XCURSOR_DEFAULT, 1);
	if (xcursor) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server->xwayland, image->buffer,
			image->width * 4, image->width,
			image->height, image->hotspot_x,
			image->hotspot_y);
	}
}

void
xwayland_reset_cursor(struct server *server)
{
	/*
	 * As xwayland caches the pixel data when not yet started up
	 * due to the delayed lazy startup approach, we do have to
	 * re-set the xwayland cursor image. Otherwise the first X11
	 * client connected will cause the xwayland server to use
	 * the cached (and potentially destroyed) pixel data.
	 *
	 * Calling this function after reloading the cursor theme
	 * ensures that the cached pixel data keeps being valid.
	 *
	 * To reproduce:
	 * - Compile with b_sanitize=address,undefined
	 * - Start labwc (nothing in autostart that could create
	 *   a X11 connection, e.g. no GTK or X11 application)
	 * - Reconfigure
	 * - Start some X11 client
	 */

	if (!server->xwayland) {
		return;
	}

	struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
		server->seat.xcursor_manager, XCURSOR_DEFAULT, 1);

	if (xcursor && !server->xwayland->xwm) {
		/* Prevents setting the cursor on an active xwayland server */
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server->xwayland, image->buffer,
			image->width * 4, image->width,
			image->height, image->hotspot_x,
			image->hotspot_y);
		return;
	}

	if (server->xwayland->cursor) {
		/*
		 * The previous configured theme has set the
		 * default cursor or the xwayland server is
		 * currently running but still has a cached
		 * xcursor set that will be used on the next
		 * xwayland destroy -> lazy startup cycle.
		 */
		zfree(server->xwayland->cursor);
	}
}

void
xwayland_server_finish(struct server *server)
{
	struct wlr_xwayland *xwayland = server->xwayland;
	wl_list_remove(&server->xwayland_new_surface.link);
	wl_list_remove(&server->xwayland_server_ready.link);
	wl_list_remove(&server->xwayland_xwm_ready.link);

	/*
	 * Reset server->xwayland to NULL first to prevent callbacks (like
	 * server_global_filter) from accessing it as it is destroyed
	 */
	server->xwayland = NULL;
	wlr_xwayland_destroy(xwayland);
}

static bool
intervals_overlap(int start_a, int end_a, int start_b, int end_b)
{
	/* check for empty intervals */
	if (end_a <= start_a || end_b <= start_b) {
		return false;
	}

	return start_a < start_b ?
		start_b < end_a :  /* B starts within A */
		start_a < end_b;   /* A starts within B */
}

/*
 * Subtract the area of an XWayland view (e.g. panel) from the usable
 * area of the output based on _NET_WM_STRUT_PARTIAL property.
 */
void
xwayland_adjust_usable_area(struct view *view, struct wlr_output_layout *layout,
		struct wlr_output *output, struct wlr_box *usable)
{
	assert(view);
	assert(layout);
	assert(output);
	assert(usable);

	if (view->type != LAB_XWAYLAND_VIEW) {
		return;
	}

	xcb_ewmh_wm_strut_partial_t *strut =
		xwayland_surface_from_view(view)->strut_partial;
	if (!strut) {
		return;
	}

	/* these are layout coordinates */
	struct wlr_box lb = { 0 };
	wlr_output_layout_get_box(layout, NULL, &lb);
	struct wlr_box ob = { 0 };
	wlr_output_layout_get_box(layout, output, &ob);

	/*
	 * strut->right/bottom are offsets from the lower right corner
	 * of the X11 screen, which should generally correspond with the
	 * lower right corner of the output layout
	 */
	double strut_left = strut->left;
	double strut_right = (lb.x + lb.width) - strut->right;
	double strut_top = strut->top;
	double strut_bottom = (lb.y + lb.height) - strut->bottom;

	/* convert layout to output coordinates */
	wlr_output_layout_output_coords(layout, output,
		&strut_left, &strut_top);
	wlr_output_layout_output_coords(layout, output,
		&strut_right, &strut_bottom);

	/* deal with right/bottom rather than width/height */
	int usable_right = usable->x + usable->width;
	int usable_bottom = usable->y + usable->height;

	/* here we mix output and layout coordinates; be careful */
	if (strut_left > usable->x && strut_left < usable_right
			&& intervals_overlap(ob.y, ob.y + ob.height,
			strut->left_start_y, strut->left_end_y + 1)) {
		usable->x = strut_left;
	}
	if (strut_right > usable->x && strut_right < usable_right
			&& intervals_overlap(ob.y, ob.y + ob.height,
			strut->right_start_y, strut->right_end_y + 1)) {
		usable_right = strut_right;
	}
	if (strut_top > usable->y && strut_top < usable_bottom
			&& intervals_overlap(ob.x, ob.x + ob.width,
			strut->top_start_x, strut->top_end_x + 1)) {
		usable->y = strut_top;
	}
	if (strut_bottom > usable->y && strut_bottom < usable_bottom
			&& intervals_overlap(ob.x, ob.x + ob.width,
			strut->bottom_start_x, strut->bottom_end_x + 1)) {
		usable_bottom = strut_bottom;
	}

	usable->width = usable_right - usable->x;
	usable->height = usable_bottom - usable->y;
}

void
xwayland_update_workarea(struct server *server)
{
	/*
	 * Do nothing if called during destroy or before xwayland is ready.
	 * This function will be called again from the ready signal handler.
	 */
	if (!server->xwayland || !server->xwayland->xwm) {
		return;
	}

	struct wlr_box lb;
	wlr_output_layout_get_box(server->output_layout, NULL, &lb);

	/* Compute outer edges of layout (excluding negative regions) */
	int layout_left = MAX(0, lb.x);
	int layout_right = MAX(0, lb.x + lb.width);
	int layout_top = MAX(0, lb.y);
	int layout_bottom = MAX(0, lb.y + lb.height);

	/* Workarea is initially the entire layout */
	int workarea_left = layout_left;
	int workarea_right = layout_right;
	int workarea_top = layout_top;
	int workarea_bottom = layout_bottom;

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (!output_is_usable(output)) {
			continue;
		}

		struct wlr_box ob;
		wlr_output_layout_get_box(server->output_layout,
			output->wlr_output, &ob);

		/* Compute edges of output */
		int output_left = ob.x;
		int output_right = ob.x + ob.width;
		int output_top = ob.y;
		int output_bottom = ob.y + ob.height;

		/* Compute edges of usable area */
		int usable_left = output_left + output->usable_area.x;
		int usable_right = usable_left + output->usable_area.width;
		int usable_top = output_top + output->usable_area.y;
		int usable_bottom = usable_top + output->usable_area.height;

		/*
		 * Only adjust workarea edges for output edges that are
		 * aligned with outer edges of layout
		 */
		if (output_left == layout_left) {
			workarea_left = MAX(workarea_left, usable_left);
		}
		if (output_right == layout_right) {
			workarea_right = MIN(workarea_right, usable_right);
		}
		if (output_top == layout_top) {
			workarea_top = MAX(workarea_top, usable_top);
		}
		if (output_bottom == layout_bottom) {
			workarea_bottom = MIN(workarea_bottom, usable_bottom);
		}
	}

	/*
	 * Set _NET_WORKAREA property. We don't report virtual desktops
	 * to XWayland, so we set only one workarea.
	 */
	struct wlr_box workarea = {
		.x = workarea_left,
		.y = workarea_top,
		.width = workarea_right - workarea_left,
		.height = workarea_bottom - workarea_top,
	};
	wlr_xwayland_set_workareas(server->xwayland, &workarea, 1);
}

void
xwayland_flush(struct server *server)
{
	if (!server->xwayland || !server->xwayland->xwm) {
		return;
	}

	xcb_flush(wlr_xwayland_get_xwm_connection(server->xwayland));
}
