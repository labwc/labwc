#include "labwc.h"

void server_new_input(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised by the backend when a new input device becomes
	 * available.
	 */
	struct server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		keyboard_new(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		cursor_new(server, device);
		break;
	default:
		break;
	}

	/*
	 * We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client.
	 */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

void seat_request_cursor(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, request_cursor);
	/*
	 * This event is rasied by the seat when a client provides a cursor
	 * image
	 */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;

	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use
		 * the provided surface as the cursor image. It will set the
		 * hardware cursor on the output that it's currently on and
		 * continue to do so as the cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface,
				       event->hotspot_x, event->hotspot_y);
	}
}

void seat_request_set_selection(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}
