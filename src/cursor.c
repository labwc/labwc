// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <linux/input-event-codes.h>
#include <sys/time.h>
#include <time.h>
#include <wlr/types/wlr_primary_selection.h>
#include "labwc.h"
#include "menu/menu.h"
#include "resistance.h"
#include "ssd.h"
#include "config/mousebind.h"

void
cursor_rebase(struct seat *seat, uint32_t time_msec)
{
	double sx, sy;
	struct wlr_surface *surface;
	int view_area = LAB_SSD_NONE;

	desktop_surface_and_view_at(seat->server, seat->cursor->x,
		seat->cursor->y, &surface, &sx, &sy, &view_area);

	if (surface) {
		wlr_seat_pointer_notify_clear_focus(seat->seat);
		wlr_seat_pointer_notify_enter(seat->seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat->seat, time_msec, sx, sy);
	} else {
		cursor_set(seat, "left_ptr");
		wlr_seat_pointer_notify_clear_focus(seat->seat);
	}
}

static void
request_cursor_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, request_cursor);
	/*
	 * This event is rasied by the seat when a client provides a cursor
	 * image
	 */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		seat->seat->pointer_state.focused_client;

	/*
	 * This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first.
	 */
	if (focused_client == event->seat_client) {
		/*
		 * Once we've vetted the client, we can tell the cursor to use
		 * the provided surface as the cursor image. It will set the
		 * hardware cursor on the output that it's currently on and
		 * continue to do so as the cursor moves between outputs.
		 */
		wlr_cursor_set_surface(seat->cursor, event->surface,
				       event->hotspot_x, event->hotspot_y);
	}
}

static void
request_set_selection_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(
		listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat->seat, event->source,
		event->serial);
}

static void
request_set_primary_selection_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(
		listener, seat, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat->seat, event->source,
		event->serial);
}

static void
request_start_drag_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(
		listener, seat, request_start_drag);
	struct wlr_seat_request_start_drag_event *event = data;
	if (wlr_seat_validate_pointer_grab_serial(seat->seat, event->origin, event->serial)) {
		wlr_seat_start_pointer_drag(seat->seat, event->drag, event->serial);
	} else {
		wlr_data_source_destroy(event->drag->source);
	}
}

static void
process_cursor_move(struct server *server, uint32_t time)
{
	damage_all_outputs(server);
	double dx = server->seat.cursor->x - server->grab_x;
	double dy = server->seat.cursor->y - server->grab_y;
	struct view *view = server->grabbed_view;

	/* Move the grabbed view to the new position. */
	dx += server->grab_box.x;
	dy += server->grab_box.y;
	resistance_move_apply(view, &dx, &dy);
	view_move(view, dx, dy);
}

static void
process_cursor_resize(struct server *server, uint32_t time)
{
	damage_all_outputs(server);
	double dx = server->seat.cursor->x - server->grab_x;
	double dy = server->seat.cursor->y - server->grab_y;

	struct view *view = server->grabbed_view;
	struct wlr_box new_view_geo = {
		.x = view->x, .y = view->y, .width = view->w, .height = view->h
	};

	int min_width, min_height;
	view_min_size(view, &min_width, &min_height);

	if (server->resize_edges & WLR_EDGE_TOP) {
		if (server->grab_box.height - dy < min_height) {
			dy = server->grab_box.height - min_height;
		}
		new_view_geo.y = server->grab_box.y + dy;
		new_view_geo.height = server->grab_box.height - dy;
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		if (server->grab_box.height + dy < min_height) {
			dy = min_height - server->grab_box.height;
		}
		new_view_geo.height = server->grab_box.height + dy;
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		if (server->grab_box.width - dx < min_width) {
			dx = server->grab_box.width - min_width;
		}
		new_view_geo.x = server->grab_box.x + dx;
		new_view_geo.width = server->grab_box.width - dx;
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		if (server->grab_box.width + dx < min_width) {
			dx = min_width - server->grab_box.width;
		}
		new_view_geo.width = server->grab_box.width + dx;
	}

	resistance_resize_apply(view, &new_view_geo);
	view_move_resize(view, new_view_geo);
}

void
cursor_set(struct seat *seat, const char *cursor_name)
{
	wlr_xcursor_manager_set_cursor_image(
		seat->xcursor_manager, cursor_name, seat->cursor);
}

bool
input_inhibit_blocks_surface(struct seat *seat, struct wl_resource *resource)
{
	struct wl_client *inhibiting_client =
		seat->active_client_while_inhibited;
	return (inhibiting_client != NULL) &&
		inhibiting_client != wl_resource_get_client(resource);
}

static void
process_cursor_motion(struct server *server, uint32_t time)
{
	static bool cursor_name_set_by_server;

	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->input_mode == LAB_INPUT_STATE_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->input_mode == LAB_INPUT_STATE_RESIZE) {
		process_cursor_resize(server, time);
		return;
	} else if (server->input_mode == LAB_INPUT_STATE_MENU) {
		menu_set_selected(server->rootmenu,
			server->seat.cursor->x, server->seat.cursor->y);
		damage_all_outputs(server);
		return;
	}

	/* Otherwise, find view under the pointer and send the event along */
	double sx, sy;
	struct wlr_seat *wlr_seat = server->seat.seat;
	struct wlr_surface *surface = NULL;
	int view_area = LAB_SSD_NONE;
	struct view *view = desktop_surface_and_view_at(server,
		server->seat.cursor->x, server->seat.cursor->y, &surface,
		&sx, &sy, &view_area);

	if (!view) {
		cursor_set(&server->seat, XCURSOR_DEFAULT);
	} else {
		uint32_t resize_edges = ssd_resize_edges(view_area);
		if (resize_edges) {
			cursor_name_set_by_server = true;
			cursor_set(&server->seat,
				wlr_xcursor_get_resize_name(resize_edges));
		} else if (view_area != LAB_SSD_NONE) {
			/* title and buttons */
			cursor_set(&server->seat, XCURSOR_DEFAULT);
			cursor_name_set_by_server = true;
		} else if (cursor_name_set_by_server) {
			cursor_set(&server->seat, XCURSOR_DEFAULT);
			cursor_name_set_by_server = false;
		}
	}


	if (view && rc.focus_follow_mouse) {
		desktop_focus_and_activate_view(&server->seat, view);
		if (rc.raise_on_focus) {
			desktop_raise_view(view);
		}
	}

	/* Required for iconify/maximize/close button mouse-over deco */
	damage_all_outputs(server);

	if (surface &&
	    !input_inhibit_blocks_surface(&server->seat, surface->resource)) {
		bool focus_changed =
			wlr_seat->pointer_state.focused_surface != surface;
		/*
		 * "Enter" the surface if necessary. This lets the client know
		 * that the cursor has entered one of its surfaces.
		 *
		 * Note that this gives the surface "pointer focus", which is
		 * distinct from keyboard focus. You get pointer focus by moving
		 * the pointer over a window.
		 */
		wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
		if (!focus_changed || server->seat.drag_icon) {
			/*
			 * The enter event contains coordinates, so we only need
			 * to notify on motion if the focus did not change.
			 */
			wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
		}
	} else {
		/*
		 * Clear pointer focus so future button events and such are not
		 * sent to the last client to have the cursor over it.
		 */
		wlr_seat_pointer_clear_focus(wlr_seat);
	}
}

void
start_drag(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, start_drag);
	struct wlr_drag *wlr_drag = data;
	seat->drag_icon = wlr_drag->icon;
	wl_signal_add(&seat->drag_icon->events.destroy, &seat->destroy_drag);
}

void
handle_constraint_commit(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, constraint_commit);
	struct wlr_pointer_constraint_v1 *constraint = seat->current_constraint;
	assert(constraint->surface = data);
}

void
destroy_constraint(struct wl_listener *listener, void *data)
{
	struct constraint *constraint = wl_container_of(listener, constraint,
		destroy);
	struct wlr_pointer_constraint_v1 *wlr_constraint = data;
	struct seat *seat = constraint->seat;

	wl_list_remove(&constraint->destroy.link);
	if (seat->current_constraint == wlr_constraint) {
		if (seat->constraint_commit.link.next != NULL) {
			wl_list_remove(&seat->constraint_commit.link);
		}
		wl_list_init(&seat->constraint_commit.link);
		seat->current_constraint = NULL;
	}

	free(constraint);
}

void
create_constraint(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_constraint_v1 *wlr_constraint = data;
	struct server *server = wl_container_of(listener, server,
		new_constraint);
	struct view *view;
	struct constraint *constraint = calloc(1, sizeof(struct constraint));

	constraint->constraint = wlr_constraint;
	constraint->seat = &server->seat;
	constraint->destroy.notify = destroy_constraint;
	wl_signal_add(&wlr_constraint->events.destroy, &constraint->destroy);

	view = desktop_focused_view(server);
	if (view->surface == wlr_constraint->surface) {
		constrain_cursor(server, wlr_constraint);
	}
}

void
constrain_cursor(struct server *server, struct wlr_pointer_constraint_v1
		*constraint)
{
	struct seat *seat = &server->seat;
	if (seat->current_constraint == constraint) {
		return;
	}
	wl_list_remove(&seat->constraint_commit.link);
	if (seat->current_constraint) {
		wlr_pointer_constraint_v1_send_deactivated(
			seat->current_constraint);
	}

	seat->current_constraint = constraint;

	if (constraint == NULL) {
		wl_list_init(&seat->constraint_commit.link);
		return;
	}

	wlr_pointer_constraint_v1_send_activated(constraint);
	seat->constraint_commit.notify = handle_constraint_commit;
	wl_signal_add(&constraint->surface->events.commit,
		&seat->constraint_commit);
}

static void
cursor_motion(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta)
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_motion);
	struct server *server = seat->server;
	struct wlr_event_pointer_motion *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	wlr_relative_pointer_manager_v1_send_relative_motion(
		server->relative_pointer_manager,
		seat->seat, (uint64_t)event->time_msec * 1000,
		event->delta_x, event->delta_y, event->unaccel_dx,
		event->unaccel_dy);
	if (!seat->current_constraint) {
		/*
		 * The cursor doesn't move unless we tell it to. The cursor
		 * automatically handles constraining the motion to the output layout,
		 * as well as any special configuration applied for the specific input
		 * device which generated the event. You can pass NULL for the device
		 * if you want to move the cursor around without any input.
		 */
		wlr_cursor_move(seat->cursor, event->device, event->delta_x,
			event->delta_y);
	}
	process_cursor_motion(seat->server, event->time_msec);
}

void
destroy_drag(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, destroy_drag);

	if (!seat->drag_icon) {
		return;
	}
	seat->drag_icon = NULL;
	desktop_focus_topmost_mapped_view(seat->server);
}

void
cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an
	 * _absolute_ motion event, from 0..1 on each axis. This happens, for
	 * example, when wlroots is running under a Wayland window rather than
	 * KMS+DRM, and you move the mouse over the window. You could enter the
	 * window from any edge, so we have to warp the mouse there. There is
	 * also some hardware which emits these events.
	 */
	struct seat *seat = wl_container_of(
		listener, seat, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, event->device,
			event->x, event->y, &lx, &ly);

	double dx = lx - seat->cursor->x;
	double dy = ly - seat->cursor->y;

	wlr_relative_pointer_manager_v1_send_relative_motion(
		seat->server->relative_pointer_manager,
		seat->seat, (uint64_t)event->time_msec * 1000,
		dx, dy, dx, dy);

	if (!seat->current_constraint) {
		/*
		 * The cursor doesn't move unless we tell it to. The cursor
		 * automatically handles constraining the motion to the output layout,
		 * as well as any special configuration applied for the specific input
		 * device which generated the event. You can pass NULL for the device
		 * if you want to move the cursor around without any input.
		 */
		wlr_cursor_move(seat->cursor, event->device, dx, dy);
	}

	process_cursor_motion(seat->server, event->time_msec);
}

static void
handle_cursor_button_with_meta_key(struct view *view, uint32_t button,
		double lx, double ly)
{
	if (!view) {
		return;
	}

	/* move */
	if (button == BTN_LEFT) {
		interactive_begin(view, LAB_INPUT_STATE_MOVE, 0);
		return;
	}

	/* resize */
	uint32_t edges;
	edges = lx < view->x + view->w / 2 ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
	edges |= ly < view->y + view->h / 2 ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
	interactive_begin(view, LAB_INPUT_STATE_RESIZE, edges);
}

static void
handle_release_mousebinding(struct server *server, uint32_t button, enum ssd_part_type view_area)
{
	struct mousebind *mousebind;
	wl_list_for_each_reverse(mousebind, &rc.mousebinds, link) {
		if (mousebind->context == view_area
				&& mousebind->button == button) {
			if (mousebind->mouse_event
					== MOUSE_ACTION_RELEASE) {
				action(server, mousebind->action,
					mousebind->command);
			}
			if (mousebind->pressed_in_context) {
				mousebind->pressed_in_context = false;
				action(server, mousebind->action,
					mousebind->command);
			}
		}
	}
}

static bool
is_double_click(long double_click_speed)
{
	static struct timespec last_double_click;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	long ms = (now.tv_sec - last_double_click.tv_sec) * 1000 +
		(now.tv_nsec - last_double_click.tv_nsec) / 1000000;
	last_double_click = now;
	return ms < double_click_speed && ms >= 0;
}

static bool
handle_press_mousebinding(struct server *server, uint32_t button, enum ssd_part_type view_area)
{
	struct mousebind *mousebind;
	bool double_click = is_double_click(rc.doubleclick_time);
	bool bound;

	wl_list_for_each_reverse(mousebind, &rc.mousebinds, link) {
		if (mousebind->context == view_area
				&& mousebind->button == button) {
			if (mousebind->mouse_event
					== MOUSE_ACTION_PRESS) {
				bound = true;
				action(server, mousebind->action,
					mousebind->command);
			} else if (mousebind->mouse_event
					== MOUSE_ACTION_CLICK) {
				bound = true;
				mousebind->pressed_in_context = true;
			} else if (double_click
					&& mousebind->mouse_event
					== MOUSE_ACTION_DOUBLECLICK) {
				bound = true;
				action(server, mousebind->action,
						mousebind->command);
			}
		}
	}
	return bound;
}

void
cursor_button(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits a button
	 * event.
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_button);
	struct server *server = seat->server;
	struct wlr_event_pointer_button *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	double sx, sy;
	struct wlr_surface *surface;
	int view_area = LAB_SSD_NONE;
	uint32_t resize_edges;

	struct view *view = desktop_surface_and_view_at(server,
		server->seat.cursor->x, server->seat.cursor->y, &surface,
		&sx, &sy, &view_area);

	/* handle alt + _press_ on view */
	struct wlr_input_device *device = seat->keyboard_group->input_device;
	uint32_t modifiers = wlr_keyboard_get_modifiers(device->keyboard);
	if (modifiers & WLR_MODIFIER_ALT && event->state == WLR_BUTTON_PRESSED && !seat->current_constraint) {
		handle_cursor_button_with_meta_key(view, event->button,
			server->seat.cursor->x, server->seat.cursor->y);
		return;
	};

	/* Notify client with pointer focus of button press */
	wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
		event->button, event->state);

	/* handle _release_ */
	if (event->state == WLR_BUTTON_RELEASED) {
		if (server->input_mode == LAB_INPUT_STATE_MENU) {
			return;
		}
		damage_all_outputs(server);
		if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
			/* Exit interactive move/resize/menu mode. */
			server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
			cursor_rebase(&server->seat, event->time_msec);
			return;
		}
		goto mousebindings;
	}

	if (server->input_mode == LAB_INPUT_STATE_MENU) {
		menu_action_selected(server, server->rootmenu);
		server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
		cursor_rebase(&server->seat, event->time_msec);
		return;
	}

	/* Handle _press_ on a layer surface */
	if (!view && surface) {
		if (!wlr_surface_is_layer_surface(surface)) {
			return;
		}
		struct wlr_layer_surface_v1 *layer =
			wlr_layer_surface_v1_from_wlr_surface(surface);
		if (layer->current.keyboard_interactive) {
			seat_set_focus_layer(&server->seat, layer);
		}
		return;
	}

	/* Handle _press_ on root window */
	if (!view) {
		action(server, "ShowMenu", "root-menu");
		return;
	}

	/* Handle _press_ on view */
	desktop_focus_and_activate_view(&server->seat, view);
	desktop_raise_view(view);
	damage_all_outputs(server);

	resize_edges = ssd_resize_edges(view_area);
	if (resize_edges) {
		interactive_begin(view, LAB_INPUT_STATE_RESIZE, resize_edges);
		return;
	}

mousebindings:
	if (event->state == WLR_BUTTON_RELEASED) {
		handle_release_mousebinding(server, event->button, view_area);
	} else if (event->state == WLR_BUTTON_PRESSED) {
		handle_press_mousebinding(server, event->button, view_area);
	}
}

void
cursor_axis(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an axis
	 * event, for example when you move the scroll wheel.
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_axis);
	struct wlr_event_pointer_axis *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat->seat, event->time_msec,
		event->orientation, event->delta, event->delta_discrete,
		event->source);
}

void
cursor_frame(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen
	 * at the same time, in which case a frame event won't be sent in
	 * between.
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat->seat);
}

void
cursor_init(struct seat *seat)
{
	seat->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(seat->xcursor_manager, 1);

	seat->cursor_motion.notify = cursor_motion;
	wl_signal_add(&seat->cursor->events.motion, &seat->cursor_motion);
	seat->cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&seat->cursor->events.motion_absolute,
		      &seat->cursor_motion_absolute);
	seat->cursor_button.notify = cursor_button;
	wl_signal_add(&seat->cursor->events.button, &seat->cursor_button);
	seat->cursor_axis.notify = cursor_axis;
	wl_signal_add(&seat->cursor->events.axis, &seat->cursor_axis);
	seat->cursor_frame.notify = cursor_frame;
	wl_signal_add(&seat->cursor->events.frame, &seat->cursor_frame);

	seat->request_cursor.notify = request_cursor_notify;
	wl_signal_add(&seat->seat->events.request_set_cursor,
		&seat->request_cursor);
	seat->request_set_selection.notify = request_set_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_selection,
		&seat->request_set_selection);
	seat->request_start_drag.notify = request_start_drag_notify;
	wl_signal_add(&seat->seat->events.request_start_drag,
		&seat->request_start_drag);
	seat->start_drag.notify = start_drag;
	wl_signal_add(&seat->seat->events.start_drag,
		&seat->start_drag);
	seat->destroy_drag.notify = destroy_drag;

	seat->request_set_primary_selection.notify =
		request_set_primary_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_primary_selection,
		&seat->request_set_primary_selection);
}
