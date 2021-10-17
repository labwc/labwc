// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <strings.h>
#include <wlr/backend/libinput.h>
#include <wlr/util/log.h>
#include "labwc.h"

static void
input_device_destroy(struct wl_listener *listener, void *data)
{
	struct input *input = wl_container_of(listener, input, destroy);
	wl_list_remove(&input->link);
	free(input);
}

static void
configure_libinput(struct wlr_input_device *wlr_input_device)
{
	if (!wlr_input_device) {
		wlr_log(WLR_ERROR, "no wlr_input_device");
		return;
	}
	struct libinput_device *libinput_dev =
		wlr_libinput_get_device_handle(wlr_input_device);
	if (!libinput_dev) {
		wlr_log(WLR_ERROR, "no libinput_dev");
		return;
	}

	enum device_type current_type;
	if (wlr_input_device->width_mm) {
		current_type = TOUCH_DEVICE;
	} else {
		current_type = NON_TOUCH_DEVICE;
	}

	struct libinput_category *device_category, *dc = NULL;
	wl_list_for_each(device_category, &rc.libinput_categories, link) {
		if (device_category->name) {
			if (!strcasecmp(wlr_input_device->name, device_category->name)) {
				dc = device_category;
				break;
			}
		} else if (device_category->type == current_type) {
			dc = device_category;
		} else if (device_category->type == DEFAULT_DEVICE && !dc) {
			dc = device_category;
		}
	}

	if (!dc) {
		wlr_log(WLR_INFO, "Skipping libinput configuration for device");
		return;
	}

	if (libinput_device_config_tap_get_finger_count(libinput_dev) <= 0) {
		wlr_log(WLR_INFO, "tap unavailable");
	} else {
		wlr_log(WLR_INFO, "tap configured");
		libinput_device_config_tap_set_enabled(libinput_dev, dc->tap);
	}

	if (libinput_device_config_scroll_has_natural_scroll(libinput_dev) <= 0
		|| dc->natural_scroll < 0) {
		wlr_log(WLR_INFO, "natural scroll not configured");
	} else {
		wlr_log(WLR_INFO, "natural scroll configured");
		libinput_device_config_scroll_set_natural_scroll_enabled(
			libinput_dev, dc->natural_scroll);
	}

	if (libinput_device_config_left_handed_is_available(libinput_dev) <= 0
		|| dc->left_handed < 0) {
		wlr_log(WLR_INFO, "left-handed mode not configured");
	} else {
		wlr_log(WLR_INFO, "left-handed mode configured");
		libinput_device_config_left_handed_set(libinput_dev,
			dc->left_handed);
	}

	if (libinput_device_config_accel_is_available(libinput_dev) == 0) {
		wlr_log(WLR_INFO, "pointer acceleration unavailable");
	} else {
		wlr_log(WLR_INFO, "pointer acceleration configured");
		if (dc->pointer_speed > -1) {
			libinput_device_config_accel_set_speed(libinput_dev,
				dc->pointer_speed);
		}
		if (dc->accel_profile > 0) {
			libinput_device_config_accel_set_profile(libinput_dev,
				dc->accel_profile);
		}
	}

	if (libinput_device_config_middle_emulation_is_available(libinput_dev)
			== 0 || dc->middle_emu < 0)  {
		wlr_log(WLR_INFO, "middle emulation not configured");
	} else {
		wlr_log(WLR_INFO, "middle emulation configured");
		libinput_device_config_middle_emulation_set_enabled(
			libinput_dev, dc->middle_emu);
	}

	if (libinput_device_config_dwt_is_available(libinput_dev) == 0
		|| dc->dwt < 0) {
		wlr_log(WLR_INFO, "dwt not configured");
	} else {
		wlr_log(WLR_INFO, "dwt configured");
		libinput_device_config_dwt_set_enabled(libinput_dev, dc->dwt);
	}
}

void
new_pointer(struct seat *seat, struct input *input)
{
	if (wlr_input_device_is_libinput(input->wlr_input_device)) {
		configure_libinput(input->wlr_input_device);
	}
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

	wl_list_init(&seat->constraint_commit.link);
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
seat_reconfigure(struct server *server)
{
	struct seat *seat = &server->seat;
	struct input *input;
	struct wlr_keyboard *kb = &seat->keyboard_group->keyboard;
	wl_list_for_each(input, &seat->inputs, link) {
		/* We don't configure keyboards by libinput, so skip them */
		if (wlr_input_device_is_libinput(input->wlr_input_device) &&
			input->wlr_input_device->type ==
			WLR_INPUT_DEVICE_POINTER) {
			configure_libinput(input->wlr_input_device);
		}
	}
	wlr_keyboard_set_repeat_info(kb, rc.repeat_rate, rc.repeat_delay);
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

	struct server *server = seat->server;
	struct wlr_pointer_constraint_v1 *constraint =
		wlr_pointer_constraints_v1_constraint_for_surface(server->constraints,
			surface, seat->seat);
	constrain_cursor(server, constraint);
}

void
seat_set_focus_layer(struct seat *seat, struct wlr_layer_surface_v1 *layer)
{
	if (!layer) {
		seat->focused_layer = NULL;
		desktop_focus_topmost_mapped_view(seat->server);
		damage_all_outputs(seat->server);
		return;
	}
	seat_focus_surface(seat, layer->surface);
	if (layer->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
		seat->focused_layer = layer;
	}
}
