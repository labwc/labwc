// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "dnd.h"
#include "input/cursor.h"
#include "labwc.h"  /* for struct seat */
#include "view.h"

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
		/* Cleans up automatically on drag->icon->events.destroy */
		wlr_scene_drag_icon_create(seat->drag.icons, drag->icon);
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
	 * The default focus behaviour at the end of a dnd operation is that the
	 * window that originally had keyboard-focus retains that focus. This is
	 * consistent with the default behaviour of openbox and mutter.
	 *
	 * However, if the 'focus/followMouse' option is enabled we need to
	 * refocus the current surface under the cursor because keyboard focus
	 * is not changed during drag.
	 */
	if (!rc.focus_follow_mouse) {
		return;
	}

	struct cursor_context ctx = get_cursor_context(seat->server);
	if (!ctx.surface) {
		return;
	}
	seat_focus_surface(seat, NULL);
	seat_focus_surface(seat, ctx.surface);

	if (ctx.view && rc.raise_on_focus) {
		view_move_to_front(ctx.view);
	}
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
