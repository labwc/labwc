#include <assert.h>
#include <wlr/util/log.h>
#include "labwc.h"

static void
input_device_destroy(struct wl_listener *listener, void *data)
{
	struct input *input = wl_container_of(listener, input, destroy);
	wl_list_remove(&input->link);
	free(input);
}

void
new_pointer(struct seat *seat, struct input *input)
{
	/* TODO: Configure libinput on device to set tap, acceleration, etc */
	wlr_cursor_attach_input_device(seat->cursor, input->wlr_input_device);
}

void
new_keyboard(struct seat *seat, struct input *input)
{
	struct wlr_keyboard *kb = input->wlr_input_device->keyboard;
	wlr_keyboard_set_keymap(kb, seat->keyboard_group->keyboard.keymap);
	wlr_keyboard_group_add_keyboard(seat->keyboard_group, kb);
	wlr_seat_set_keyboard(seat->seat, input->wlr_input_device);
}

static void
new_input_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, new_input);
	struct wlr_input_device *device = data;
	struct input *input = calloc(1, sizeof(struct input));
	input->wlr_input_device = device;
	input->seat = seat;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		new_keyboard(seat, input);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		new_pointer(seat, input);
		break;
	default:
		wlr_log(WLR_INFO, "unsupported input device");
		break;
	}

	input->destroy.notify = input_device_destroy;
	wl_signal_add(&device->events.destroy, &input->destroy);
	wl_list_insert(&seat->inputs, &input->link);

	uint32_t caps = 0;
	wl_list_for_each(input, &seat->inputs, link) {
		switch (input->wlr_input_device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			caps |= WL_SEAT_CAPABILITY_KEYBOARD;
			break;
		case WLR_INPUT_DEVICE_POINTER:
			caps |= WL_SEAT_CAPABILITY_POINTER;
			break;
		default:
			break;
		}
	}
	wlr_seat_set_capabilities(seat->seat, caps);
}

void
seat_init(struct server *server)
{
	struct seat *seat = &server->seat;
	seat->server = server;

	seat->seat = wlr_seat_create(server->wl_display, "seat0");
	if (!seat->seat) {
		wlr_log(WLR_ERROR, "cannot allocate seat");
		exit(EXIT_FAILURE);
	}

	wl_list_init(&seat->inputs);
	seat->new_input.notify = new_input_notify;
	wl_signal_add(&server->backend->events.new_input, &seat->new_input);

	seat->cursor = wlr_cursor_create();
	if (!seat->cursor) {
		wlr_log(WLR_ERROR, "unable to create cursor");
		exit(EXIT_FAILURE);
	}
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	keyboard_init(seat);
	cursor_init(seat);
}

void
seat_finish(struct server *server)
{
	struct seat *seat = &server->seat;
	wl_list_remove(&seat->cursor_motion.link);
	wl_list_remove(&seat->cursor_motion_absolute.link);
	wl_list_remove(&seat->cursor_button.link);
	wl_list_remove(&seat->cursor_axis.link);
	wl_list_remove(&seat->cursor_frame.link);
	wl_list_remove(&seat->request_cursor.link);
	wl_list_remove(&seat->request_set_selection.link);
	wl_list_remove(&seat->new_input.link);
	if (seat->keyboard_group) {
		wlr_keyboard_group_destroy(seat->keyboard_group);
	}
	wlr_xcursor_manager_destroy(seat->xcursor_manager);
	wlr_cursor_destroy(seat->cursor);
}

void
seat_focus_surface(struct seat *seat, struct wlr_surface *surface)
{
	if (!surface) {
		wlr_seat_keyboard_notify_clear_focus(seat->seat);
		return;
	}
	struct wlr_keyboard *kb = &seat->keyboard_group->keyboard;
	wlr_seat_keyboard_notify_enter(seat->seat, surface, kb->keycodes,
		kb->num_keycodes, &kb->modifiers);
}
