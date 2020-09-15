#include "labwc.h"

static void process_cursor_move(struct server *server, uint32_t time)
{
	/* Move the grabbed view to the new position. */
	double dx = server->cursor->x - server->grab_x;
	double dy = server->cursor->y - server->grab_y;
	server->grabbed_view->x = server->grab_box.x + dx;
	server->grabbed_view->y = server->grab_box.y + dy;

	if (server->grabbed_view->type != LAB_XWAYLAND_VIEW)
		return;

	struct view *view = server->grabbed_view;
	wlr_xwayland_surface_configure(view->xwayland_surface, view->x, view->y,
				       view->xwayland_surface->width,
				       view->xwayland_surface->height);
}

#define MIN_VIEW_WIDTH (100)
#define MIN_VIEW_HEIGHT (60)

static void process_cursor_resize(struct server *server, uint32_t time)
{
	/*
	 * TODO: Wait for the client to prepare a buffer at the new size, then
	 * commit any movement that was prepared.
	 */
	double dx = server->cursor->x - server->grab_x;
	double dy = server->cursor->y - server->grab_y;

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
	    (new_view_geo.width < MIN_VIEW_WIDTH))
		return;

	/* Move */
	view->x = new_view_geo.x;
	view->y = new_view_geo.y;

	/* Resize */
	new_view_geo.width -= 2 * view->xdg_grab_offset;
	new_view_geo.height -= 2 * view->xdg_grab_offset;
	view_resize(view, new_view_geo);
}

static void process_cursor_motion(struct server *server, uint32_t time)
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
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	int view_area;
	struct view *view = desktop_view_at(server, server->cursor->x,
					    server->cursor->y, &surface, &sx,
					    &sy, &view_area);
	if (!view) {
		/* If there's no view under the cursor, set the cursor image to
		 * a default. This is what makes the cursor image appear when
		 * you move it around the screen, not over any views. */
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, XCURSOR_DEFAULT, server->cursor);
	}

	/* TODO: Could we use wlr_xcursor_get_resize_name() here?? */
	switch (view_area) {
	case LAB_DECO_PART_TITLE:
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, XCURSOR_DEFAULT, server->cursor);
		break;
	case LAB_DECO_PART_TOP:
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, "top_side", server->cursor);
		break;
	case LAB_DECO_PART_RIGHT:
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, "right_side", server->cursor);
		break;
	case LAB_DECO_PART_BOTTOM:
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, "bottom_side", server->cursor);
		break;
	case LAB_DECO_PART_LEFT:
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, "left_side", server->cursor);
		break;
	}
	if (surface) {
		bool focus_changed = seat->pointer_state.focused_surface !=
				     surface;
		/*
		 * "Enter" the surface if necessary. This lets the client know
		 * that the cursor has entered one of its surfaces.
		 *
		 * Note that this gives the surface "pointer focus", which is
		 * distinct from keyboard focus. You get pointer focus by moving
		 * the pointer over a window.
		 */
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		if (!focus_changed) {
			/* The enter event contains coordinates, so we only need
			 * to notify on motion if the focus did not change. */
			wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		}
	} else {
		/* Clear pointer focus so future button events and such are not
		 * sent to the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}

void cursor_motion(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta)
	 */
	struct server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;

	/*
	 * The cursor doesn't move unless we tell it to. The cursor
	 * automatically handles constraining the motion to the output layout,
	 * as well as any special configuration applied for the specific input
	 * device which generated the event. You can pass NULL for the device
	 * if you want to move the cursor around without any input.
	 */
	wlr_cursor_move(server->cursor, event->device, event->delta_x,
			event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

void cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an
	 * _absolute_ motion event, from 0..1 on each axis. This happens, for
	 * example, when wlroots is running under a Wayland window rather than
	 * KMS+DRM, and you move the mouse over the window. You could enter the
	 * window from any edge, so we have to warp the mouse there. There is
	 * also some hardware which emits these events.
	 */
	struct server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(server->cursor, event->device, event->x,
				 event->y);
	process_cursor_motion(server, event->time_msec);
}

void cursor_button(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits a button
	 * event.
	 */
	struct server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_event_pointer_button *event = data;

	/*
	 * Notify the client with pointer focus that a button press has
	 * occurred.
	 */
	wlr_seat_pointer_notify_button(server->seat, event->time_msec,
				       event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface;
	int view_area;
	struct view *view = desktop_view_at(server, server->cursor->x,
					    server->cursor->y, &surface, &sx,
					    &sy, &view_area);
	if (event->state == WLR_BUTTON_RELEASED) {
		/* Exit interactive move/resize mode. */
		server->cursor_mode = LAB_CURSOR_PASSTHROUGH;
	} else {
		/* Focus that client if the button was _pressed_ */
		desktop_focus_view(view);
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

void cursor_axis(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an axis
	 * event, for example when you move the scroll wheel.
	 */
	struct server *server = wl_container_of(listener, server, cursor_axis);
	struct wlr_event_pointer_axis *event = data;

	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(server->seat, event->time_msec,
				     event->orientation, event->delta,
				     event->delta_discrete, event->source);
}

void cursor_frame(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen
	 * at the same time, in which case a frame event won't be sent in
	 * between.
	 */
	struct server *server = wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

void cursor_new(struct server *server, struct wlr_input_device *device)
{
	/* TODO: Configure libinput on device to set tap, acceleration, etc */
	wlr_cursor_attach_input_device(server->cursor, device);
}
