#include "labwc.h"

static void process_cursor_move(struct server *server, uint32_t time)
{
	/* Move the grabbed view to the new position. */
	server->grabbed_view->x = server->cursor->x - server->grab_x;
	server->grabbed_view->y = server->cursor->y - server->grab_y;
}

static void process_cursor_resize(struct server *server, uint32_t time)
{
	/*
	 * Resizing the grabbed view can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the
	 * view on one or two axes, but can also move the view if you resize
	 * from the top or left edges (or top-left corner).
	 *
	 * Note that I took some shortcuts here. In a more fleshed-out
	 * compositor, you'd wait for the client to prepare a buffer at the new
	 * size, then commit any movement that was prepared.
	 */
	struct view *view = server->grabbed_view;
	double dx = server->cursor->x - server->grab_x;
	double dy = server->cursor->y - server->grab_y;
	double x = view->x;
	double y = view->y;
	int width = server->grab_width;
	int height = server->grab_height;
	if (server->resize_edges & WLR_EDGE_TOP) {
		y = server->grab_y + dy;
		height -= dy;
		if (height < 1) {
			y += height;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		height += dy;
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		x = server->grab_x + dx;
		width -= dx;
		if (width < 1) {
			x += width;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		width += dx;
	}
	view->x = x;
	view->y = y;
	wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
}

static void process_cursor_motion(struct server *server, uint32_t time)
{
	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->cursor_mode == TINYWL_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == TINYWL_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Otherwise, find the view under the pointer and send the event along.
	 */
	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct view *view = desktop_view_at(server, server->cursor->x,
					    server->cursor->y, &surface, &sx,
					    &sy);
	if (!view) {
		/* If there's no view under the cursor, set the cursor image to
		 * a default. This is what makes the cursor image appear when
		 * you move it around the screen, not over any views. */
		wlr_xcursor_manager_set_cursor_image(
			server->cursor_mgr, "left_ptr", server->cursor);
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

void server_cursor_motion(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta) */
	struct server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor
	 * automatically handles constraining the motion to the output layout,
	 * as well as any special configuration applied for the specific input
	 * device which generated the event. You can pass NULL for the device if
	 * you want to move the cursor around without any input. */
	wlr_cursor_move(server->cursor, event->device, event->delta_x,
			event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an
	 * _absolute_ motion event, from 0..1 on each axis. This happens, for
	 * example, when wlroots is running under a Wayland window rather than
	 * KMS+DRM, and you move the mouse over the window. You could enter the
	 * window from any edge, so we have to warp the mouse there. There is
	 * also some hardware which emits these events. */
	struct server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(server->cursor, event->device, event->x,
				 event->y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_button(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_event_pointer_button *event = data;
	/* Notify the client with pointer focus that a button press has occurred
	 */
	wlr_seat_pointer_notify_button(server->seat, event->time_msec,
				       event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface;
	struct view *view = desktop_view_at(server, server->cursor->x,
					    server->cursor->y, &surface, &sx,
					    &sy);
	if (event->state == WLR_BUTTON_RELEASED) {
		/* If you released any buttons, we exit interactive move/resize
		 * mode. */
		server->cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
	} else {
		/* Focus that client if the button was _pressed_ */
		focus_view(view, surface);
	}
}

void server_cursor_axis(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an axis
	 * event, for example when you move the scroll wheel. */
	struct server *server = wl_container_of(listener, server, cursor_axis);
	struct wlr_event_pointer_axis *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(server->seat, event->time_msec,
				     event->orientation, event->delta,
				     event->delta_discrete, event->source);
}

void server_cursor_frame(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at
	 * the
	 * same time, in which case a frame event won't be sent in between. */
	struct server *server = wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

void server_new_output(struct wl_listener *listener, void *data)
{
	/* This event is rasied by the backend when a new output (aka a display
	 * or monitor) becomes available. */
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/* Some backends don't have modes. DRM+KMS does, and we need to set a
	 * mode before we can use the output. The mode is a tuple of (width,
	 * height, refresh rate), and each monitor supports only a specific set
	 * of modes. We just pick the first, a more sophisticated compositor
	 * would let the user configure it or pick the mode the display
	 * advertises as preferred. */
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);
	}

	/* Allocates and configures our state for this output */
	struct output *output = calloc(1, sizeof(struct output));
	output->wlr_output = wlr_output;
	output->server = server;
	/* Sets up a listener for the frame notify event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);

	/* Adds this to the output layout. The add_auto function arranges
	 * outputs from left-to-right in the order they appear. A more
	 * sophisticated compositor would let the user configure the arrangement
	 * of outputs in the layout. */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);

	/* Creating the global adds a wl_output global to the display, which
	 * Wayland clients can see to find out information about the output
	 * (such as DPI, scale factor, manufacturer, etc). */
	wlr_output_create_global(wlr_output);
}
