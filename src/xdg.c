// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include "common/mem.h"
#include "decorations.h"
#include "labwc.h"
#include "node.h"
#include "view.h"
#include "view-impl-common.h"
#include "window-rules.h"
#include "workspaces.h"

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

static void
handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_view *xdg_toplevel_view =
		wl_container_of(listener, xdg_toplevel_view, new_popup);
	struct view *view = &xdg_toplevel_view->base;
	struct wlr_xdg_popup *wlr_popup = data;
	xdg_popup_create(view, wlr_popup);
}

static bool
has_ssd(struct view *view)
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

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	assert(view->surface);

	struct wlr_box size;
	wlr_xdg_surface_get_geometry(xdg_surface, &size);

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

	view_impl_apply_geometry(view, view->current.width,
		view->current.height);

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
handle_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, map);
	view->impl->map(view);
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, unmap);
	view->impl->unmap(view, /* client_request */ true);
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
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
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
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
	 * want.
	 */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_resize);
	interactive_begin(view, LAB_INPUT_STATE_RESIZE, event->edges);
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
	view_maximize(view, xdg_toplevel_from_view(view)->requested.maximized,
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
		/*
		 * We can't assume here that view->current is equal to
		 * view->pending because some clients (e.g. terminals)
		 * refuse to accept the exact size we requested. To
		 * account for the size difference and avoid visual
		 * glitches during resize, we apply the same position
		 * adjustments here as in handle_commit().
		 */
		view_impl_apply_geometry(view, view->current.width,
			view->current.height);
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

enum z_direction {
	LAB_TO_FRONT,
	LAB_TO_BACK,
};

/*
 * TODO: Combine append_children() and move_sub_views() as much as possible.
 * https://github.com/labwc/labwc/pull/998#discussion_r1284085575
 */
static void
move_sub_views(struct view *parent, enum z_direction z_direction)
{
	assert(parent);

	if (parent->type != LAB_XDG_SHELL_VIEW) {
		return;
	}

	struct wlr_xdg_toplevel *parent_toplevel = xdg_toplevel_from_view(parent);
	struct view *view, *next;
	wl_list_for_each_reverse_safe(view, next, &parent->server->views, link)
	{
		/* need to stop here, otherwise loops keeps going forever */
		if (view == parent) {
			break;
		}
		if (view->type != LAB_XDG_SHELL_VIEW) {
			continue;
		}
		if (!view->mapped && !view->minimized) {
			continue;
		}
		if (top_parent_of(view) != parent_toplevel) {
			continue;
		}
		if (z_direction == LAB_TO_FRONT) {
			view_impl_move_to_front(view);
		} else if (z_direction == LAB_TO_BACK) {
			view_impl_move_to_back(view);
		}
	}
}

/* Return the most senior parent (=root) view */
static struct view *
xdg_toplevel_view_get_root(struct view *view)
{
	struct wlr_xdg_toplevel *root = top_parent_of(view);
	struct wlr_xdg_surface *surface = (struct wlr_xdg_surface *)root->base;
	return (struct view *)surface->data;
}

/*
 * In the view_move_to_{front,back} functions, a modal dialog is always shown
 * above its parent window, and the two always move together, so other window
 * cannot come between them.
 * This is consistent with GTK3/Qt5 applications on mutter and openbox.
 */
static void
xdg_toplevel_view_move_to_front(struct view *view)
{
	struct view *root = xdg_toplevel_view_get_root(view);
	view_impl_move_to_front(root);
	move_sub_views(root, LAB_TO_FRONT);
}

static void
xdg_toplevel_view_move_to_back(struct view *view)
{
	struct view *root = xdg_toplevel_view_get_root(view);
	view_impl_move_to_back(root);
	move_sub_views(root, LAB_TO_BACK);
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
position_xdg_toplevel_view(struct view *view)
{
	struct wlr_xdg_toplevel *parent_xdg_toplevel =
		xdg_toplevel_from_view(view)->parent;

	if (!parent_xdg_toplevel) {
		view_center(view, NULL);
	} else {
		/*
		 * If child-toplevel-views, we center-align relative to their
		 * parents
		 */
		struct view *parent = lookup_view_by_xdg_toplevel(
			view->server, parent_xdg_toplevel);
		assert(parent);
		view_set_output(view, parent->output);
		view_center(view, &parent->pending);
	}
}

static const char *
xdg_toplevel_view_get_string_prop(struct view *view, const char *prop)
{
	struct wlr_xdg_toplevel *xdg_toplevel = xdg_toplevel_from_view(view);
	if (!strcmp(prop, "title")) {
		return xdg_toplevel->title;
	}
	if (!strcmp(prop, "app_id")) {
		return xdg_toplevel->app_id;
	}
	return "";
}

static void
xdg_toplevel_view_map(struct view *view)
{
	if (view->mapped) {
		return;
	}
	view->mapped = true;
	if (!view->output) {
		view_set_output(view, output_nearest_to_cursor(view->server));
	}
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	view->surface = xdg_surface->surface;
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	if (!view->been_mapped) {
		struct wlr_xdg_toplevel_requested *requested =
			&xdg_toplevel_from_view(view)->requested;
		foreign_toplevel_handle_create(view);
		view_set_decorations(view, has_ssd(view));

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
			position_xdg_toplevel_view(view);
		}

		if (!view->fullscreen && requested->fullscreen) {
			set_fullscreen_from_request(view, requested);
		} else if (!view->maximized && requested->maximized) {
			view_maximize(view, true,
				/*store_natural_geometry*/ true);
		}

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
		desktop_focus_topmost_mapped_view(view->server);
	}
}

static const struct view_impl xdg_toplevel_view_impl = {
	.configure = xdg_toplevel_view_configure,
	.close = xdg_toplevel_view_close,
	.get_string_prop = xdg_toplevel_view_get_string_prop,
	.map = xdg_toplevel_view_map,
	.set_activated = xdg_toplevel_view_set_activated,
	.set_fullscreen = xdg_toplevel_view_set_fullscreen,
	.unmap = xdg_toplevel_view_unmap,
	.maximize = xdg_toplevel_view_maximize,
	.minimize = xdg_toplevel_view_minimize,
	.move_to_front = xdg_toplevel_view_move_to_front,
	.move_to_back = xdg_toplevel_view_move_to_back,
	.get_root = xdg_toplevel_view_get_root,
	.append_children = xdg_toplevel_view_append_children,
};

void
xdg_activation_handle_request(struct wl_listener *listener, void *data)
{
	const struct wlr_xdg_activation_v1_request_activate_event *event = data;

	if (!wlr_surface_is_xdg_surface(event->surface)) {
		return;
	}
	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_from_wlr_surface(event->surface);
	struct view *view = xdg_surface ? xdg_surface->data : NULL;

	if (!view) {
		wlr_log(WLR_INFO, "Not activating surface - no view attached to surface");
		return;
	}
	if (!event->token->seat) {
		wlr_log(WLR_INFO, "Denying focus request, seat wasn't supplied");
		return;
	}
	/*
	 * We do not check for event->token->surface here because it may already
	 * be destroyed and thus being NULL. With wlroots 0.17 we can hook into
	 * the `new_token` signal, attach further information to the token and
	 * then react to that information here instead. For now we just check
	 * for the seat / serial being correct and then allow the request.
	 */

	/*
	 * TODO: This is the exact same code as used in foreign.c.
	 *       Refactor it into a public helper function somewhere.
	 */
	wlr_log(WLR_DEBUG, "Activating surface");
	if (view->workspace != view->server->workspace_current) {
		workspaces_switch_to(view->workspace);
	}
	desktop_focus_and_activate_view(&view->server->seat, view);
	view_move_to_front(view);
}

/*
 * We use the following struct user_data pointers:
 *   - wlr_xdg_surface->data = view
 *     for the wlr_xdg_toplevel_decoration_v1 implementation
 *   - wlr_surface->data = scene_tree
 *     to help the popups find their parent nodes
 */
void
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

	/* In support of xdg popups */
	xdg_surface->surface->data = tree;

	view->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	view->request_move.notify = handle_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = handle_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_minimize.notify = handle_request_minimize;
	wl_signal_add(&toplevel->events.request_minimize, &view->request_minimize);
	view->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
	view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);

	view->set_title.notify = handle_set_title;
	wl_signal_add(&toplevel->events.set_title, &view->set_title);

	/* Events specific to XDG toplevel views */
	xdg_toplevel_view->set_app_id.notify = handle_set_app_id;
	wl_signal_add(&toplevel->events.set_app_id, &xdg_toplevel_view->set_app_id);

	xdg_toplevel_view->new_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&xdg_surface->events.new_popup, &xdg_toplevel_view->new_popup);

	wl_list_insert(&server->views, &view->link);
}
