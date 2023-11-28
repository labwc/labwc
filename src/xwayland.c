// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <wlr/xwayland.h>
#include "common/array.h"
#include "common/macros.h"
#include "common/mem.h"
#include "labwc.h"
#include "node.h"
#include "ssd.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"
#include "workspaces.h"
#include "xwayland.h"

static void xwayland_view_unmap(struct view *view, bool client_request);

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

	switch (wlr_xwayland_icccm_input_model(xsurface)) {
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
	 *
	 * [Currently, labwc does not fully support clients voluntarily
	 * taking focus via the WM_TAKE_FOCUS + SetInputFocus mechanism.
	 * Instead, we try to guess whether the window wants focus based
	 * on some heuristics -- see below.]
	 */
	case WLR_ICCCM_INPUT_MODEL_GLOBAL:
		/*
		 * Assume the window does want focus if it wants window
		 * decorations (according to _MOTIF_WM_HINTS). This is
		 * a stop-gap fix to ensure that various applications
		 * (mainly Java-based ones such as IntelliJ IDEA) get
		 * focus normally and appear in the window switcher. It
		 * would be better to match based on _NET_WM_WINDOW_TYPE
		 * instead, but that property isn't currently available
		 * through wlroots API.
		 */
		return (xsurface->decorations ==
			WLR_XWAYLAND_SURFACE_DECORATIONS_ALL) ?
			VIEW_WANTS_FOCUS_ALWAYS : VIEW_WANTS_FOCUS_OFFER;

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

static struct wlr_xwayland_surface *
top_parent_of(struct view *view)
{
	struct wlr_xwayland_surface *s = xwayland_surface_from_view(view);
	while (s->parent) {
		s = s->parent;
	}
	return s;
}

static struct xwayland_view *
xwayland_view_from_view(struct view *view)
{
	assert(view->type == LAB_XWAYLAND_VIEW);
	return (struct xwayland_view *)view;
}

struct wlr_xwayland_surface *
xwayland_surface_from_view(struct view *view)
{
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	assert(xwayland_view->xwayland_surface);
	return xwayland_view->xwayland_surface;
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
	interactive_begin(view, LAB_INPUT_STATE_MOVE, 0);
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
	interactive_begin(view, LAB_INPUT_STATE_RESIZE, event->edges);
}

static void
handle_associate(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, associate);
	assert(xwayland_view->xwayland_surface &&
		xwayland_view->xwayland_surface->surface);

	view_connect_map(&xwayland_view->base,
		xwayland_view->xwayland_surface->surface);
}

static void
handle_dissociate(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, dissociate);

	mappable_disconnect(&xwayland_view->base.mappable);
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, surface_destroy);
	assert(data && data == view->surface);

	view->surface = NULL;
	wl_list_remove(&view->surface_destroy.link);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	assert(xwayland_view->xwayland_surface->data == view);

	if (view->surface) {
		/*
		 * We got the destroy signal from
		 * wlr_xwayland_surface before the
		 * destroy signal from wlr_surface.
		 */
		wl_list_remove(&view->surface_destroy.link);
	}
	view->surface = NULL;

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

	if (view_is_floating(view)) {
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
	if (!view->mapped) {
		ensure_initial_geometry_and_output(view);
		/*
		 * Set decorations early to avoid changing geometry
		 * after maximize (reduces visual glitches).
		 */
		view_set_decorations(view,
			want_deco(xwayland_surface_from_view(view)));
	}
	view_toggle_maximize(view, VIEW_AXIS_BOTH);
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
	view_update_title(view);
}

static void
handle_set_class(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_class);
	struct view *view = &xwayland_view->base;
	view_update_app_id(view);
}

static void
xwayland_view_close(struct view *view)
{
	wlr_xwayland_surface_close(xwayland_surface_from_view(view));
}

static const char *
xwayland_view_get_string_prop(struct view *view, const char *prop)
{
	struct xwayland_view *xwayland_view = xwayland_view_from_view(view);
	struct wlr_xwayland_surface *xwayland_surface = xwayland_view->xwayland_surface;
	if (!xwayland_surface) {
		/*
		 * This may happen due to a matchOnce rule when
		 * a view is destroyed while A-Tab is open. See
		 * https://github.com/labwc/labwc/issues/1082#issuecomment-1716137180
		 */
		return "";
	}

	if (!strcmp(prop, "title")) {
		return xwayland_surface->title;
	}
	if (!strcmp(prop, "class")) {
		return xwayland_surface->class;
	}
	/* We give 'class' for wlr_foreign_toplevel_handle_v1_set_app_id() */
	if (!strcmp(prop, "app_id")) {
		return xwayland_surface->class;
	}
	return "";
}

static void
handle_set_decorations(struct wl_listener *listener, void *data)
{
	struct xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_decorations);
	struct view *view = &xwayland_view->base;

	view_set_decorations(view, want_deco(xwayland_view->xwayland_surface));
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
		xwayland_view_unmap(view, /* client_request */ true);
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
set_initial_position(struct view *view,
		struct wlr_xwayland_surface *xwayland_surface)
{
	/* Don't center views with position explicitly specified */
	bool has_position = xwayland_surface->size_hints &&
		(xwayland_surface->size_hints->flags & (
			XCB_ICCCM_SIZE_HINT_US_POSITION |
			XCB_ICCCM_SIZE_HINT_P_POSITION));

	if (has_position) {
		/*
		 * Make sure a floating view is onscreen. For a
		 * maximized/fullscreen view, do nothing; if it is
		 * unmaximized/leaves fullscreen later, we will make
		 * sure it is on-screen at that point.
		 */
		if (view_is_floating(view)) {
			view_adjust_for_layout_change(view);
		}
	} else {
		if (view_is_floating(view)) {
			view_center(view, NULL);
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
}

static void
init_foreign_toplevel(struct view *view)
{
	foreign_toplevel_handle_create(view);

	struct wlr_xwayland_surface *surface = xwayland_surface_from_view(view);
	if (!surface->parent) {
		return;
	}
	struct view *parent = (struct view *)surface->parent->data;
	if (!parent->toplevel.handle) {
		return;
	}
	wlr_foreign_toplevel_handle_v1_set_parent(view->toplevel.handle, parent->toplevel.handle);
}

static void
xwayland_view_map(struct view *view)
{
	struct wlr_xwayland_surface *xwayland_surface = xwayland_surface_from_view(view);
	if (view->mapped) {
		return;
	}
	if (!xwayland_surface->surface) {
		/*
		 * We may get here if a user minimizes an xwayland dialog at the
		 * same time as the client requests unmap, which xwayland
		 * clients sometimes do without actually requesting destroy
		 * even if they don't intend to use that view/surface anymore
		 */
		wlr_log(WLR_DEBUG, "Cannot map view without wlr_surface");
		return;
	}
	view->mapped = true;
	ensure_initial_geometry_and_output(view);
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);

	if (view->surface != xwayland_surface->surface) {
		if (view->surface) {
			wl_list_remove(&view->surface_destroy.link);
		}
		view->surface = xwayland_surface->surface;

		/* Required to set the surface to NULL when destroyed by the client */
		view->surface_destroy.notify = handle_surface_destroy;
		wl_signal_add(&view->surface->events.destroy, &view->surface_destroy);

		/* Will be free'd automatically once the surface is being destroyed */
		struct wlr_scene_tree *tree = wlr_scene_subsurface_tree_create(
			view->scene_tree, view->surface);
		if (!tree) {
			/* TODO: might need further clean up */
			wl_resource_post_no_memory(view->surface->resource);
			return;
		}
		view->scene_node = &tree->node;
	}

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
	view_set_fullscreen(view, xwayland_surface->fullscreen);
	if (!view->been_mapped) {
		view_set_decorations(view, want_deco(xwayland_surface));
	}
	enum view_axis axis = VIEW_AXIS_NONE;
	if (xwayland_surface->maximized_horz) {
		axis |= VIEW_AXIS_HORIZONTAL;
	}
	if (xwayland_surface->maximized_vert) {
		axis |= VIEW_AXIS_VERTICAL;
	}
	view_maximize(view, axis, /*store_natural_geometry*/ true);

	if (!view->toplevel.handle) {
		init_foreign_toplevel(view);
	}

	if (!view->been_mapped) {
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

	/* Add commit here, as xwayland map/unmap can change the wlr_surface */
	wl_signal_add(&xwayland_surface->surface->events.commit, &view->commit);
	view->commit.notify = handle_commit;

	view_impl_map(view);
	view->been_mapped = true;

	/* Update usable area to account for XWayland "struts" (panels) */
	if (xwayland_surface->strut_partial) {
		output_update_all_usable_areas(view->server, false);
	}
}

static void
xwayland_view_unmap(struct view *view, bool client_request)
{
	if (!view->mapped) {
		goto out;
	}
	view->mapped = false;
	wl_list_remove(&view->commit.link);
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	view_impl_unmap(view);

	/* Update usable area to account for XWayland "struts" (panels) */
	if (xwayland_surface_from_view(view)->strut_partial) {
		output_update_all_usable_areas(view->server, false);
	}

	/*
	 * If the view was explicitly unmapped by the client (rather
	 * than just minimized), destroy the foreign toplevel handle so
	 * the unmapped view doesn't show up in panels and the like.
	 */
out:
	if (client_request && view->toplevel.handle) {
		wlr_foreign_toplevel_handle_v1_destroy(view->toplevel.handle);
		view->toplevel.handle = NULL;
	}
}

static void
xwayland_view_maximize(struct view *view, bool maximized)
{
	wlr_xwayland_surface_set_maximized(xwayland_surface_from_view(view),
		maximized);
}

static void
xwayland_view_minimize(struct view *view, bool minimized)
{
	wlr_xwayland_surface_set_minimized(xwayland_surface_from_view(view),
		minimized);
}

static void
xwayland_view_move_to_front(struct view *view)
{
	view_impl_move_to_front(view);
	/*
	 * Update XWayland stacking order.
	 *
	 * FIXME: it would be better to restack above the next lower
	 * view, rather than on top of all other surfaces. Restacking
	 * the unmanaged surfaces afterward is ugly and still doesn't
	 * account for always-on-top views.
	 */
	wlr_xwayland_surface_restack(xwayland_surface_from_view(view),
		NULL, XCB_STACK_MODE_ABOVE);
	/* Restack unmanaged surfaces on top */
	struct wl_list *list = &view->server->unmanaged_surfaces;
	struct xwayland_unmanaged *u;
	wl_list_for_each(u, list, link) {
		wlr_xwayland_surface_restack(u->xwayland_surface,
			NULL, XCB_STACK_MODE_ABOVE);
	}
}

static void
xwayland_view_move_to_back(struct view *view)
{
	view_impl_move_to_back(view);
	/* Update XWayland stacking order */
	wlr_xwayland_surface_restack(xwayland_surface_from_view(view),
		NULL, XCB_STACK_MODE_BELOW);
}

static struct view *
xwayland_view_get_root(struct view *view)
{
	struct wlr_xwayland_surface *root = top_parent_of(view);
	return (struct view *)root->data;
}

static void
xwayland_view_append_children(struct view *self, struct wl_array *children)
{
	struct wlr_xwayland_surface *surface = xwayland_surface_from_view(self);
	struct view *view;

	wl_list_for_each_reverse(view, &self->server->views, link)
	{
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
		if (!view->mapped && !view->minimized) {
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
xwayland_view_is_related(struct view *view, struct wlr_surface *surface)
{
	struct wlr_xwayland_surface *xsurface =
		xwayland_surface_from_view(view);
	struct wlr_xwayland_surface *xsurface2 =
		wlr_xwayland_surface_try_from_wlr_surface(surface);

	return (xsurface2 && xsurface2->pid == xsurface->pid);
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

static const struct view_impl xwayland_view_impl = {
	.configure = xwayland_view_configure,
	.close = xwayland_view_close,
	.get_string_prop = xwayland_view_get_string_prop,
	.map = xwayland_view_map,
	.set_activated = xwayland_view_set_activated,
	.set_fullscreen = xwayland_view_set_fullscreen,
	.unmap = xwayland_view_unmap,
	.maximize = xwayland_view_maximize,
	.minimize = xwayland_view_minimize,
	.move_to_front = xwayland_view_move_to_front,
	.move_to_back = xwayland_view_move_to_back,
	.get_root = xwayland_view_get_root,
	.append_children = xwayland_view_append_children,
	.is_related = xwayland_view_is_related,
	.get_size_hints = xwayland_view_get_size_hints,
	.wants_focus = xwayland_view_wants_focus,
	.has_strut_partial = xwayland_view_has_strut_partial,
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

	/*
	 * Set two-way view <-> xsurface association.  Usually the association
	 * remains until the xsurface is destroyed (which also destroys the
	 * view).  The only exception is caused by setting override-redirect on
	 * the xsurface, which removes it from the view (destroying the view)
	 * and makes it an "unmanaged" surface.
	 */
	xwayland_view->xwayland_surface = xsurface;
	xsurface->data = view;

	view->workspace = server->workspace_current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	node_descriptor_create(&view->scene_tree->node, LAB_NODE_DESC_VIEW, view);

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

	wl_list_insert(&view->server->views, &view->link);

	if (mapped) {
		xwayland_view_map(view);
	}
}

static void
handle_new_surface(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, xwayland_new_surface);
	struct wlr_xwayland_surface *xsurface = data;
	wlr_xwayland_surface_ping(xsurface);

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

static void
handle_ready(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, xwayland_ready);
	wlr_xwayland_set_seat(server->xwayland, server->seat.seat);
	xwayland_update_workarea(server);
}

void
xwayland_server_init(struct server *server, struct wlr_compositor *compositor)
{
	server->xwayland =
		wlr_xwayland_create(server->wl_display, compositor, true);
	if (!server->xwayland) {
		wlr_log(WLR_ERROR, "cannot create xwayland server");
		exit(EXIT_FAILURE);
	}
	server->xwayland_new_surface.notify = handle_new_surface;
	wl_signal_add(&server->xwayland->events.new_surface,
		&server->xwayland_new_surface);

	server->xwayland_ready.notify = handle_ready;
	wl_signal_add(&server->xwayland->events.ready,
		&server->xwayland_ready);

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

/*
 * Until we expose the workspaces to xwayland we need a way to
 * ensure that xwayland views on the current workspace are always
 * stacked above xwayland views on other workspaces.
 *
 * If we fail to do so, issues arise in scenarios where we change
 * the mouse focus but do not change the (xwayland) stacking order.
 *
 * Reproducer:
 * - open at least two xwayland windows which allow scrolling
 *   (some X11 terminal with 'man man' for example)
 * - switch to another workspace, open another xwayland
 *   window which allows scrolling and maximize it
 * - switch back to the previous workspace with the two windows
 * - move the mouse to the xwayland window that does *not* have focus
 * - start scrolling
 * - all scroll events should end up on the maximized window on the other workspace
 */
void
xwayland_adjust_stacking_order(struct server *server)
{
	struct view **view;
	struct wl_array views;

	wl_array_init(&views);
	view_array_append(server, &views, LAB_VIEW_CRITERIA_ALWAYS_ON_TOP);
	view_array_append(server, &views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE
		| LAB_VIEW_CRITERIA_NO_ALWAYS_ON_TOP);

	/*
	 * view_array_append() provides top-most windows
	 * first so we simply reverse the iteration here
	 */
	wl_array_for_each_reverse(view, &views) {
		view_move_to_front(*view);
	}

	wl_array_release(&views);
}

void
xwayland_server_finish(struct server *server)
{
	struct wlr_xwayland *xwayland = server->xwayland;
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
