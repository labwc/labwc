// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <linux/input-event-codes.h>
#include <sys/time.h>
#include <time.h>
#include <wlr/types/wlr_primary_selection.h>
#include "action.h"
#include "labwc.h"
#include "menu/menu.h"
#include "resistance.h"
#include "ssd.h"
#include "config/mousebind.h"
#include "common/scene-helpers.h"
#include "common/zfree.h"

/* Used to prevent setting the same cursor image twice */
static char *last_cursor_image = NULL;

static bool
is_surface(enum ssd_part_type view_area)
{
	return view_area == LAB_SSD_CLIENT
		|| view_area == LAB_SSD_LAYER_SURFACE
#if HAVE_XWAYLAND
		|| view_area == LAB_SSD_UNMANAGED
#endif
		;
}

/*
 * cursor_rebase() for internal use: reuses node, surface, sx and sy
 * For a public variant use cursor_update_focus()
 */
static void
cursor_rebase(struct seat *seat, struct wlr_scene_node *node,
		struct wlr_surface *surface, double sx, double sy, uint32_t time_msec,
		bool force)
{
	if (seat->pressed.surface && surface != seat->pressed.surface) {
		/* Don't leave surface when a button was pressed over another surface */
		return;
	}

	if (surface) {
		if (!force && surface == seat->seat->pointer_state.focused_surface) {
			/*
			 * Usually we prevent re-entering an already focused surface
			 * because it sends useless leave and enter events.
			 *
			 * They may also seriously confuse clients if sent between
			 * connected events like a double click (#258) or fast scroll
			 * events caused by a touchpad (#225).
			 *
			 * If we just want to update the cursor image though
			 * the @force argument may be used to allow re-entering.
			 */
			return;
		}
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
	 * This event is raised by the seat when a client provides a cursor
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

		if (last_cursor_image) {
			zfree(last_cursor_image);
		}
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
	if (wlr_seat_validate_pointer_grab_serial(seat->seat, event->origin,
			event->serial)) {
		wlr_seat_start_pointer_drag(seat->seat, event->drag,
			event->serial);
	} else {
		wlr_data_source_destroy(event->drag->source);
	}
}

static void
process_cursor_move(struct server *server, uint32_t time)
{
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
	double dx = server->seat.cursor->x - server->grab_x;
	double dy = server->seat.cursor->y - server->grab_y;

	struct view *view = server->grabbed_view;
	struct wlr_box new_view_geo = {
		.x = view->x, .y = view->y, .width = view->w, .height = view->h
	};

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_view_geo.height = server->grab_box.height - dy;
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_view_geo.height = server->grab_box.height + dy;
	}

	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_view_geo.width = server->grab_box.width - dx;
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_view_geo.width = server->grab_box.width + dx;
	}

	resistance_resize_apply(view, &new_view_geo);
	view_adjust_size(view, &new_view_geo.width, &new_view_geo.height);

	if (server->resize_edges & WLR_EDGE_TOP) {
		/* anchor bottom edge */
		new_view_geo.y = server->grab_box.y +
			server->grab_box.height - new_view_geo.height;
	}

	if (server->resize_edges & WLR_EDGE_LEFT) {
		/* anchor right edge */
		new_view_geo.x = server->grab_box.x +
			server->grab_box.width - new_view_geo.width;
	}

	view_move_resize(view, new_view_geo);
}

void
cursor_set(struct seat *seat, const char *cursor_name)
{
	if (last_cursor_image) {
		if (!strcmp(last_cursor_image, cursor_name)) {
			/* Prevent setting the same cursor image twice */
			return;
		}
		free(last_cursor_image);
	}
	last_cursor_image = strdup(cursor_name);

	wlr_xcursor_manager_set_cursor_image(
		seat->xcursor_manager, cursor_name, seat->cursor);
}

bool
input_inhibit_blocks_surface(struct seat *seat, struct wl_resource *resource)
{
	struct wl_client *inhibiting_client =
		seat->active_client_while_inhibited;
	return inhibiting_client
		&& inhibiting_client != wl_resource_get_client(resource);
}

static void
process_cursor_motion_out_of_surface(struct server *server, uint32_t time)
{
	struct view *view = server->seat.pressed.view;
	struct wlr_scene_node *node = server->seat.pressed.node;
	struct wlr_surface *surface = server->seat.pressed.surface;
	assert(surface);
	int lx, ly;

	if (view) {
		lx = view->x;
		ly = view->y;
	} else if (node && wlr_surface_is_layer_surface(surface)) {
		wlr_scene_node_coords(node, &lx, &ly);
#if HAVE_XWAYLAND
	} else if (node && node->parent == server->unmanaged_tree) {
		wlr_scene_node_coords(node, &lx, &ly);
#endif
	} else {
		wlr_log(WLR_ERROR, "Can't detect surface for out-of-surface movement");
		return;
	}

	double sx = server->seat.cursor->x - lx;
	double sy = server->seat.cursor->y - ly;
	/* Take into account invisible xdg-shell CSD borders */
	if (view && view->type == LAB_XDG_SHELL_VIEW && view->xdg_surface) {
		struct wlr_box geo;
		wlr_xdg_surface_get_geometry(view->xdg_surface, &geo);
		sx += geo.x;
		sy += geo.y;
	}

	wlr_seat_pointer_notify_motion(server->seat.seat, time, sx, sy);
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
	}

	/* Otherwise, find view under the pointer and send the event along */
	double sx, sy;
	struct wlr_seat *wlr_seat = server->seat.seat;
	struct wlr_scene_node *node;
	enum ssd_part_type view_area = LAB_SSD_NONE;
	struct view *view = desktop_node_and_view_at(server,
		server->seat.cursor->x, server->seat.cursor->y, &node,
		&sx, &sy, &view_area);

	struct wlr_surface *surface = NULL;
	if (is_surface(view_area)) {
		surface = lab_wlr_surface_from_node(node);
	}

	/* resize handles */
	uint32_t resize_edges = ssd_resize_edges(view_area);

	/* Set cursor */
	if (view_area == LAB_SSD_ROOT || view_area == LAB_SSD_MENU) {
		cursor_set(&server->seat, XCURSOR_DEFAULT);
	} else if (resize_edges) {
		cursor_name_set_by_server = true;
		cursor_set(&server->seat,
			wlr_xcursor_get_resize_name(resize_edges));
	} else if (ssd_part_contains(LAB_SSD_PART_TITLEBAR, view_area)) {
		/* title and buttons */
		cursor_set(&server->seat, XCURSOR_DEFAULT);
		cursor_name_set_by_server = true;
	} else if (cursor_name_set_by_server) {
		/* xdg/xwindow window content */
		/* layershell or unmanaged */
		cursor_set(&server->seat, XCURSOR_DEFAULT);
		cursor_name_set_by_server = false;
	}

	if (view_area == LAB_SSD_MENU) {
		menu_process_cursor_motion(node);
		return;
	}

	if (view && rc.focus_follow_mouse) {
		desktop_focus_and_activate_view(&server->seat, view);
		if (rc.raise_on_focus) {
			desktop_move_to_front(view);
		}
	}

	struct mousebind *mousebind;
	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (mousebind->mouse_event == MOUSE_ACTION_DRAG
				&& mousebind->pressed_in_context) {
			/* Find closest resize edges in case action is Resize */
			if (view && !resize_edges) {
				resize_edges |= server->seat.cursor->x
					< view->x + view->w / 2 ? WLR_EDGE_LEFT
					: WLR_EDGE_RIGHT;
				resize_edges |= server->seat.cursor->y
					< view->y + view->h / 2 ? WLR_EDGE_TOP
					: WLR_EDGE_BOTTOM;
			}

			mousebind->pressed_in_context = false;
			actions_run(NULL, server, &mousebind->actions, resize_edges);
		}
	}

	/* TODO: ssd_hover_state should likely be located in server->seat */
	ssd_update_button_hover(node, &server->ssd_hover_state);

	if (server->seat.pressed.surface &&
			server->seat.pressed.surface != surface &&
			!server->seat.drag_icon) {
		/*
		 * Button has been pressed while over another surface
		 * and is still held down. Just send the adjusted motion
		 * events to the focused surface so we can keep scrolling
		 * or selecting text even if the cursor moves outside of
		 * the surface.
		 */
		process_cursor_motion_out_of_surface(server, time);
	} else if (surface && !input_inhibit_blocks_surface(
			&server->seat, surface->resource)) {
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
		 *
		 * Except if we started pressing a button on the last client and
		 * are not currently in drag and drop mode.
		 */
		wlr_seat_pointer_clear_focus(wlr_seat);
	}
}

static uint32_t
msec(const struct timespec *t)
{
	return t->tv_sec * 1000 + t->tv_nsec / 1000000;
}

void
cursor_update_focus(struct server *server)
{
	double sx, sy;
	struct wlr_scene_node *node = NULL;
	struct wlr_surface *surface = NULL;
	enum ssd_part_type view_area = LAB_SSD_NONE;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	struct seat *seat = &server->seat;
	desktop_node_and_view_at(seat->server, seat->cursor->x,
		seat->cursor->y, &node, &sx, &sy, &view_area);

	if (is_surface(view_area)) {
		surface = lab_wlr_surface_from_node(node);
	}

	ssd_update_button_hover(node, &seat->server->ssd_hover_state);

	/* Focus surface under cursor if it isn't already focused */
	cursor_rebase(seat, node, surface, sx, sy, msec(&now), false);
}

void
start_drag(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, start_drag);
	struct wlr_drag *wlr_drag = data;
	seat_reset_pressed(seat);
	seat->drag_icon = wlr_drag->icon;
	if (!seat->drag_icon) {
		wlr_log(WLR_ERROR,
			"Started drag but application did not set a drag icon");
		return;
	}
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
		if (seat->constraint_commit.link.next) {
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
	if (view && view->surface == wlr_constraint->surface) {
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

	if (!constraint) {
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
	struct wlr_pointer_motion_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	wlr_relative_pointer_manager_v1_send_relative_motion(
		server->relative_pointer_manager,
		seat->seat, (uint64_t)event->time_msec * 1000,
		event->delta_x, event->delta_y, event->unaccel_dx,
		event->unaccel_dy);
	if (!seat->current_constraint) {
		/*
		 * The cursor doesn't move unless we tell it to. The cursor
		 * automatically handles constraining the motion to the output
		 * layout, as well as any special configuration applied for the
		 * specific input device which generated the event. You can pass
		 * NULL for the device if you want to move the cursor around
		 * without any input.
		 */
		wlr_cursor_move(seat->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
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
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor,
		&event->pointer->base, event->x, event->y, &lx, &ly);

	double dx = lx - seat->cursor->x;
	double dy = ly - seat->cursor->y;

	wlr_relative_pointer_manager_v1_send_relative_motion(
		seat->server->relative_pointer_manager,
		seat->seat, (uint64_t)event->time_msec * 1000,
		dx, dy, dx, dy);

	if (!seat->current_constraint) {
		/*
		 * The cursor doesn't move unless we tell it to. The cursor
		 * automatically handles constraining the motion to the output
		 * layout, as well as any special configuration applied for the
		 * specific input device which generated the event. You can pass
		 * NULL for the device if you want to move the cursor around
		 * without any input.
		 */
		wlr_cursor_move(seat->cursor, &event->pointer->base, dx, dy);
	}

	process_cursor_motion(seat->server, event->time_msec);
}

static bool
handle_release_mousebinding(struct view *view, struct server *server,
		uint32_t button, uint32_t modifiers,
		enum ssd_part_type view_area, uint32_t resize_edges)
{
	struct mousebind *mousebind;
	bool activated_any = false;
	bool activated_any_frame = false;

	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (ssd_part_contains(mousebind->context, view_area)
				&& mousebind->button == button
				&& modifiers == mousebind->modifiers) {
			switch (mousebind->mouse_event) {
			case MOUSE_ACTION_RELEASE:
				break;
			case MOUSE_ACTION_CLICK:
				if (mousebind->pressed_in_context) {
					break;
				}
				continue;
			case MOUSE_ACTION_DRAG:
				if (mousebind->pressed_in_context) {
					/*
					 * Swallow the release event as well as
					 * the press one
					 */
					activated_any = true;
					activated_any_frame |=
						mousebind->context == LAB_SSD_FRAME;
				}
				continue;
			default:
				continue;
			}
			activated_any = true;
			activated_any_frame |= mousebind->context == LAB_SSD_FRAME;
			actions_run(view, server, &mousebind->actions, resize_edges);
		}
	}
	/*
	 * Clear "pressed" status for all bindings of this mouse button,
	 * regardless of whether activated or not
	 */
	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (mousebind->button == button) {
			mousebind->pressed_in_context = false;
		}
	}
	return activated_any && activated_any_frame;
}

static bool
is_double_click(long double_click_speed, uint32_t button, struct view *view)
{
	static uint32_t last_button;
	static struct view *last_view;
	static struct timespec last_click;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	long ms = (now.tv_sec - last_click.tv_sec) * 1000 +
		(now.tv_nsec - last_click.tv_nsec) / 1000000;
	last_click = now;
	if (last_button != button || last_view != view) {
		last_button = button;
		last_view = view;
		return false;
	}
	if (ms < double_click_speed && ms >= 0) {
		/*
		 * End sequence so that third click is not considered a
		 * double-click
		 */
		last_button = 0;
		last_view = NULL;
		return true;
	}
	return false;
}

static bool
handle_press_mousebinding(struct view *view, struct server *server,
		uint32_t button, uint32_t modifiers,
		enum ssd_part_type view_area, uint32_t resize_edges)
{
	struct mousebind *mousebind;
	bool double_click = is_double_click(rc.doubleclick_time, button, view);
	bool activated_any = false;
	bool activated_any_frame = false;

	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (ssd_part_contains(mousebind->context, view_area)
				&& mousebind->button == button
				&& modifiers == mousebind->modifiers) {
			switch (mousebind->mouse_event) {
			case MOUSE_ACTION_DRAG: /* fallthrough */
			case MOUSE_ACTION_CLICK:
				/*
				 * DRAG and CLICK actions will be processed on
				 * the release event, unless the press event is
				 * counted as a DOUBLECLICK.
				 */
				if (!double_click) {
					/*
					 * Swallow the press event as well as
					 * the release one
					 */
					activated_any = true;
					activated_any_frame |=
						mousebind->context == LAB_SSD_FRAME;
					mousebind->pressed_in_context = true;
				}
				continue;
			case MOUSE_ACTION_DOUBLECLICK:
				if (!double_click) {
					continue;
				}
				break;
			case MOUSE_ACTION_PRESS:
				break;
			default:
				continue;
			}
			activated_any = true;
			activated_any_frame |= mousebind->context == LAB_SSD_FRAME;
			actions_run(view, server, &mousebind->actions, resize_edges);
		}
	}
	return activated_any && activated_any_frame;
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
	struct wlr_pointer_button_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	double sx, sy;
	struct wlr_scene_node *node = NULL;
	enum ssd_part_type view_area = LAB_SSD_NONE;
	uint32_t resize_edges = 0;

	/**
	 * Used in WLR_BUTTON_RELEASED, set on WLR_BUTTON_PRESSED
	 *
	 * Automatically initialized with 0 / false and
	 * checkpatch.pl complains when done manually.
	 */
	static bool close_menu;

	/* bindings to the Frame context swallow mouse events if activated */
	bool triggered_frame_binding = false;

	struct view *view = desktop_node_and_view_at(server,
		server->seat.cursor->x, server->seat.cursor->y, &node,
		&sx, &sy, &view_area);

	struct wlr_surface *surface = NULL;
	if (is_surface(view_area)) {
		surface = lab_wlr_surface_from_node(node);
	}

	/* get modifiers */
	struct wlr_keyboard *keyboard = &seat->keyboard_group->keyboard;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);

	/* handle _release_ */
	if (event->state == WLR_BUTTON_RELEASED) {
		struct wlr_surface *pressed_surface = seat->pressed.surface;
		seat_reset_pressed(seat);
		if (pressed_surface && pressed_surface != surface) {
			/*
			 * Button released but originally pressed over a different surface.
			 * Just send the release event to the still focused surface.
			 */
			wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
				event->button, event->state);
			return;
		}
		if (server->input_mode == LAB_INPUT_STATE_MENU) {
			if (close_menu) {
				menu_close_root(server);
				cursor_rebase(&server->seat, node, surface, sx, sy,
					event->time_msec, false);
				close_menu = false;
			}
			return;
		}
		if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
			/* Exit interactive move/resize/menu mode. */
			if (server->grabbed_view == view) {
				interactive_end(view);
			} else {
				server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
				server->grabbed_view = NULL;
			}

			/*
			 * Focus surface under cursor and force updating the
			 * cursor icon
			 */
			cursor_rebase(&server->seat, node, surface, sx, sy,
				event->time_msec, true);
			return;
		}
		goto mousebindings;
	}

	/* Handle _press */
	if (surface) {
		seat->pressed.view = view;
		seat->pressed.node = node;
		seat->pressed.surface = surface;
	}

	if (server->input_mode == LAB_INPUT_STATE_MENU) {
		/* We are closing the menu on RELEASE to not leak a stray release */
		if (view_area != LAB_SSD_MENU) {
			close_menu = true;
		} else if (menu_call_actions(node)) {
			/* Action was successfull, may fail if item just opens a submenu */
			close_menu = true;
		}
		return;
	}

	/* Handle _press_ on a layer surface */
	if (view_area == LAB_SSD_LAYER_SURFACE) {
		struct wlr_layer_surface_v1 *layer =
			wlr_layer_surface_v1_from_wlr_surface(surface);
		if (layer->current.keyboard_interactive) {
			seat_set_focus_layer(&server->seat, layer);
		}
	}

	if (view) {
		/* Determine closest resize edges in case action is Resize */
		resize_edges = ssd_resize_edges(view_area);
		if (!resize_edges) {
			resize_edges |= server->seat.cursor->x < view->x + view->w / 2
				? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
			resize_edges |= server->seat.cursor->y < view->y + view->h / 2
				? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
		}
	}

mousebindings:
	if (event->state == WLR_BUTTON_RELEASED) {
		triggered_frame_binding |= handle_release_mousebinding(view,
			server, event->button, modifiers,
			view_area, resize_edges);
	} else if (event->state == WLR_BUTTON_PRESSED) {
		triggered_frame_binding |= handle_press_mousebinding(view,
			server, event->button, modifiers,
			view_area, resize_edges);
	}
	if (surface && !triggered_frame_binding) {
		/* Notify client with pointer focus of button press */
		wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
			event->button, event->state);
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
	struct wlr_pointer_axis_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	/* Make sure we are sending the events to the surface under the cursor */
	cursor_update_focus(seat->server);

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

static void handle_pointer_pinch_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_begin);
	struct wlr_pointer_pinch_begin_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void handle_pointer_pinch_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_update);
	struct wlr_pointer_pinch_update_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

static void handle_pointer_pinch_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_end);
	struct wlr_pointer_pinch_end_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

static void handle_pointer_swipe_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_begin);
	struct wlr_pointer_swipe_begin_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void handle_pointer_swipe_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_update);
	struct wlr_pointer_swipe_update_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy);
}

static void handle_pointer_swipe_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_end);
	struct wlr_pointer_swipe_end_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

void
cursor_init(struct seat *seat)
{
	const char *xcursor_theme = getenv("XCURSOR_THEME");
	const char *xcursor_size = getenv("XCURSOR_SIZE");
	uint32_t size = xcursor_size ? atoi(xcursor_size) : 24;

	seat->xcursor_manager = wlr_xcursor_manager_create(xcursor_theme, size);
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

	seat->pointer_gestures = wlr_pointer_gestures_v1_create(seat->server->wl_display);
	seat->pinch_begin.notify = handle_pointer_pinch_begin;
	wl_signal_add(&seat->cursor->events.pinch_begin, &seat->pinch_begin);
	seat->pinch_update.notify = handle_pointer_pinch_update;
	wl_signal_add(&seat->cursor->events.pinch_update, &seat->pinch_update);
	seat->pinch_end.notify = handle_pointer_pinch_end;
	wl_signal_add(&seat->cursor->events.pinch_end, &seat->pinch_end);
	seat->swipe_begin.notify = handle_pointer_swipe_begin;
	wl_signal_add(&seat->cursor->events.swipe_begin, &seat->swipe_begin);
	seat->swipe_update.notify = handle_pointer_swipe_update;
	wl_signal_add(&seat->cursor->events.swipe_update, &seat->swipe_update);
	seat->swipe_end.notify = handle_pointer_swipe_end;
	wl_signal_add(&seat->cursor->events.swipe_end, &seat->swipe_end);

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

void cursor_finish(struct seat *seat)
{
	wl_list_remove(&seat->cursor_motion.link);
	wl_list_remove(&seat->cursor_motion_absolute.link);
	wl_list_remove(&seat->cursor_button.link);
	wl_list_remove(&seat->cursor_axis.link);
	wl_list_remove(&seat->cursor_frame.link);

	wl_list_remove(&seat->pinch_begin.link);
	wl_list_remove(&seat->pinch_update.link);
	wl_list_remove(&seat->pinch_end.link);
	wl_list_remove(&seat->swipe_begin.link);
	wl_list_remove(&seat->swipe_update.link);
	wl_list_remove(&seat->swipe_end.link);

	wl_list_remove(&seat->request_cursor.link);
	wl_list_remove(&seat->request_set_selection.link);

	wlr_xcursor_manager_destroy(seat->xcursor_manager);
	wlr_cursor_destroy(seat->cursor);
}
