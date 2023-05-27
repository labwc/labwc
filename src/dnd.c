// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "cursor.h"
#include "dnd.h"
#include "labwc.h"  /* for struct seat */

/* Internal DnD icon handlers */
static void
handle_icon_map(struct wl_listener *listener, void *data)
{
	struct drag_icon *self = wl_container_of(listener, self, events.map);
	struct wlr_drag_icon *icon = data;
	if (icon->data) {
		struct wlr_scene_tree *surface_tree = icon->data;
		wlr_scene_node_set_enabled(&surface_tree->node, true);
	} else {
		icon->data = wlr_scene_subsurface_tree_create(
			self->icon_tree, icon->surface);
	}
}

static void
handle_surface_commit(struct wl_listener *listener, void *data)
{
	struct drag_icon *self = wl_container_of(listener, self, events.commit);
	struct wlr_surface *surface = data;
	struct wlr_scene_tree *surface_tree = self->icon->data;
	if (surface_tree) {
		wlr_scene_node_set_position(&surface_tree->node,
			surface->sx, surface->sy);
	}
}

static void
handle_icon_unmap(struct wl_listener *listener, void *data)
{
	struct drag_icon *self = wl_container_of(listener, self, events.unmap);
	struct wlr_drag_icon *icon = data;
	struct wlr_scene_tree *surface_tree = icon->data;
	if (surface_tree) {
		wlr_scene_node_set_enabled(&surface_tree->node, false);
	}
}

static void
handle_icon_destroy(struct wl_listener *listener, void *data)
{
	struct drag_icon *self = wl_container_of(listener, self, events.destroy);

	wl_list_remove(&self->events.map.link);
	wl_list_remove(&self->events.commit.link);
	wl_list_remove(&self->events.unmap.link);
	wl_list_remove(&self->events.destroy.link);

	if (self->icon->data) {
		struct wlr_scene_tree *tree = self->icon->data;
		wlr_scene_node_destroy(&tree->node);
	}

	self->icon = NULL;
	self->icon_tree = NULL;
	free(self);
}

static void
drag_icon_create(struct seat *seat, struct wlr_drag_icon *wlr_icon)
{
	assert(seat);
	assert(wlr_icon);
	struct drag_icon *self = znew(*self);

	self->icon = wlr_icon;
	self->icon_tree = seat->drag.icons;

	/* Position will be updated by cursor movement */
	wlr_scene_node_set_position(&self->icon_tree->node,
		seat->cursor->x, seat->cursor->y);
	wlr_scene_node_raise_to_top(&self->icon_tree->node);

	/* Set up events */
	self->events.map.notify = handle_icon_map;
	self->events.commit.notify = handle_surface_commit;
	self->events.unmap.notify = handle_icon_unmap;
	self->events.destroy.notify = handle_icon_destroy;

	wl_signal_add(&wlr_icon->events.map, &self->events.map);
	wl_signal_add(&wlr_icon->surface->events.commit, &self->events.commit);
	wl_signal_add(&wlr_icon->events.unmap, &self->events.unmap);
	wl_signal_add(&wlr_icon->events.destroy, &self->events.destroy);
}

/* Internal DnD handlers */
static void
handle_drag_request(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, drag.events.request);
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(
			seat->seat, event->origin, event->serial)) {
		wlr_seat_start_pointer_drag(seat->seat, event->drag,
			event->serial);
	} else {
		wlr_data_source_destroy(event->drag->source);
		wlr_log(WLR_ERROR, "wrong source for drag request");
	}
}

static void
handle_drag_start(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, drag.events.start);
	assert(!seat->drag.active);
	struct wlr_drag *drag = data;

	seat->drag.active = true;
	seat_reset_pressed(seat);
	if (drag->icon) {
		/* Cleans up automatically on drag->icon->events.detroy */
		drag_icon_create(seat, drag->icon);
		wlr_scene_node_set_enabled(&seat->drag.icons->node, true);
	}
	wl_signal_add(&drag->events.destroy, &seat->drag.events.destroy);
}

static void
handle_drag_destroy(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, drag.events.destroy);
	assert(seat->drag.active);

	seat->drag.active = false;
	wl_list_remove(&seat->drag.events.destroy.link);
	wlr_scene_node_set_enabled(&seat->drag.icons->node, false);

	/*
	 * Keyboard focus is not changed during drag, so we need to refocus the
	 * current surface under the cursor.
	 */
	struct cursor_context ctx = get_cursor_context(seat->server);
	if (!ctx.surface) {
		return;
	}
	seat_focus_surface(seat, NULL);
	seat_focus_surface(seat, ctx.surface);
}

/* Public API */
void
dnd_init(struct seat *seat)
{
	seat->drag.icons = wlr_scene_tree_create(&seat->server->scene->tree);
	wlr_scene_node_set_enabled(&seat->drag.icons->node, false);

	seat->drag.events.request.notify = handle_drag_request;
	seat->drag.events.start.notify = handle_drag_start;
	seat->drag.events.destroy.notify = handle_drag_destroy;

	wl_signal_add(&seat->seat->events.request_start_drag,
		&seat->drag.events.request);
	wl_signal_add(&seat->seat->events.start_drag, &seat->drag.events.start);
	/*
	 * destroy.notify is listened to in handle_drag_start() and reset in
	 * handle_drag_destroy()
	 */
}

void
dnd_icons_show(struct seat *seat, bool show)
{
	wlr_scene_node_set_enabled(&seat->drag.icons->node, show);
}

void
dnd_icons_move(struct seat *seat, double x, double y)
{
	wlr_scene_node_set_position(&seat->drag.icons->node, x, y);
}

void dnd_finish(struct seat *seat)
{
	wlr_scene_node_destroy(&seat->drag.icons->node);
	wl_list_remove(&seat->drag.events.request.link);
	wl_list_remove(&seat->drag.events.start.link);
}
