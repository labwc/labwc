#include "labwc.h"

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

	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use
		 * the provided surface as the cursor image. It will set the
		 * hardware cursor on the output that it's currently on and
		 * continue to do so as the cursor moves between outputs. */
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
process_cursor_move(struct server *server, uint32_t time)
{
	/* Move the grabbed view to the new position. */
	double dx = server->seat.cursor->x - server->grab_x;
	double dy = server->seat.cursor->y - server->grab_y;
	server->grabbed_view->x = server->grab_box.x + dx;
	server->grabbed_view->y = server->grab_box.y + dy;

	if (server->grabbed_view->type != LAB_XWAYLAND_VIEW) {
		return;
	}

	struct view *view = server->grabbed_view;
	wlr_xwayland_surface_configure(view->xwayland_surface, view->x, view->y,
				       view->xwayland_surface->width,
				       view->xwayland_surface->height);
}

#define MIN_VIEW_WIDTH (100)
#define MIN_VIEW_HEIGHT (60)

static void
process_cursor_resize(struct server *server, uint32_t time)
{
	/*
	 * TODO: Wait for the client to prepare a buffer at the new size, then
	 * commit any movement that was prepared.
	 */
	double dx = server->seat.cursor->x - server->grab_x;
	double dy = server->seat.cursor->y - server->grab_y;

	struct view *view = server->grabbed_view;
	struct wlr_box new_view_geo = {
		.x = view->x, .y = view->y, .width = view->w, .height = view->h
	};

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_view_geo.y = server->grab_box.y + dy;
		new_view_geo.height = server->grab_box.height - dy;
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_view_geo.height = server->grab_box.height + dy;
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_view_geo.x = server->grab_box.x + dx;
		new_view_geo.width = server->grab_box.width - dx;
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_view_geo.width = server->grab_box.width + dx;
	}
	if ((new_view_geo.height < MIN_VIEW_HEIGHT) ||
	    (new_view_geo.width < MIN_VIEW_WIDTH)) {
		return;
	}

	/* Move */
	view->x = new_view_geo.x;
	view->y = new_view_geo.y;

	/* Resize */
	new_view_geo.width -= 2 * view->xdg_grab_offset;
	new_view_geo.height -= 2 * view->xdg_grab_offset;
	view_resize(view, new_view_geo);
}

static void
process_cursor_motion(struct server *server, uint32_t time)
{
	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->cursor_mode == LAB_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == LAB_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Otherwise, find the view under the pointer and send the event along.
	 */
	double sx, sy;
	struct wlr_seat *wlr_seat = server->seat.seat;
	struct wlr_surface *surface = NULL;
	int view_area;
	struct view *view =
		desktop_view_at(server, server->seat.cursor->x, server->seat.cursor->y,
				&surface, &sx, &sy, &view_area);
	if (!view) {
		/*
		 * If there's no view under the cursor, set the cursor image to
		 * a default. This is what makes the cursor image appear when
		 * you move it around the screen, not over any views.
		 */
		wlr_xcursor_manager_set_cursor_image(
			server->seat.xcursor_manager, XCURSOR_DEFAULT, server->seat.cursor);
	}

	/* TODO: Could we use wlr_xcursor_get_resize_name() here?? */
	switch (view_area) {
	case LAB_DECO_NONE:
		break;
	case LAB_DECO_PART_TOP:
		wlr_xcursor_manager_set_cursor_image(
			server->seat.xcursor_manager, "top_side", server->seat.cursor);
		break;
	case LAB_DECO_PART_RIGHT:
		wlr_xcursor_manager_set_cursor_image(
			server->seat.xcursor_manager, "right_side", server->seat.cursor);
		break;
	case LAB_DECO_PART_BOTTOM:
		wlr_xcursor_manager_set_cursor_image(
			server->seat.xcursor_manager, "bottom_side", server->seat.cursor);
		break;
	case LAB_DECO_PART_LEFT:
		wlr_xcursor_manager_set_cursor_image(
			server->seat.xcursor_manager, "left_side", server->seat.cursor);
		break;
	default:
		wlr_xcursor_manager_set_cursor_image(
			server->seat.xcursor_manager, XCURSOR_DEFAULT, server->seat.cursor);
		break;
	}
	if (surface) {
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
		if (!focus_changed) {
			/*
			 * The enter event contains coordinates, so we only need
			 * to notify on motion if the focus did not change.
			 */
			wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
		}
	} else {
		/* Clear pointer focus so future button events and such are not
		 * sent to the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(wlr_seat);
	}
}

void
cursor_motion(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta)
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_motion);
	struct wlr_event_pointer_motion *event = data;

	/*
	 * The cursor doesn't move unless we tell it to. The cursor
	 * automatically handles constraining the motion to the output layout,
	 * as well as any special configuration applied for the specific input
	 * device which generated the event. You can pass NULL for the device
	 * if you want to move the cursor around without any input.
	 */
	wlr_cursor_move(seat->cursor, event->device, event->delta_x,
		event->delta_y);
	process_cursor_motion(seat->server, event->time_msec);
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
	wlr_cursor_warp_absolute(seat->cursor, event->device, event->x, event->y);
	process_cursor_motion(seat->server, event->time_msec);
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

	/*
	 * Notify the client with pointer focus that a button press has
	 * occurred.
	 */
	wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
		event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface;
	int view_area;
	struct view *view =
		desktop_view_at(server, server->seat.cursor->x, server->seat.cursor->y,
				&surface, &sx, &sy, &view_area);
	if (event->state == WLR_BUTTON_RELEASED) {
		/* Exit interactive move/resize mode. */
		server->cursor_mode = LAB_CURSOR_PASSTHROUGH;
	} else {
		/* Focus that client if the button was _pressed_ */
		desktop_focus_view(&server->seat, view);
		switch (view_area) {
		case LAB_DECO_BUTTON_CLOSE:
			view->impl->close(view);
			break;
		case LAB_DECO_BUTTON_ICONIFY:
			view_minimize(view);
			break;
		case LAB_DECO_PART_TITLE:
			interactive_begin(view, LAB_CURSOR_MOVE, 0);
			break;
		case LAB_DECO_PART_TOP:
			interactive_begin(view, LAB_CURSOR_RESIZE,
					  WLR_EDGE_TOP);
			break;
		case LAB_DECO_PART_RIGHT:
			interactive_begin(view, LAB_CURSOR_RESIZE,
					  WLR_EDGE_RIGHT);
			break;
		case LAB_DECO_PART_BOTTOM:
			interactive_begin(view, LAB_CURSOR_RESIZE,
					  WLR_EDGE_BOTTOM);
			break;
		case LAB_DECO_PART_LEFT:
			interactive_begin(view, LAB_CURSOR_RESIZE,
					  WLR_EDGE_LEFT);
			break;
		}
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
	wl_signal_add(&seat->seat->events.request_set_cursor, &seat->request_cursor);
	seat->request_set_selection.notify = request_set_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_selection, &seat->request_set_selection);

	/* TODO:
	 * seat->request_set_primary_selection.notify =
	 *	request_set_primary_selectioni_notify;
	 * wl_signal_add(&seat->seat->events.request_set_primary_selection,
	 *	&seat->request_set_primary_selection);
	 */
}
