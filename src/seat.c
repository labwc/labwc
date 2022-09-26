// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdbool.h>
#include <strings.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "key-state.h"
#include "labwc.h"

static void
input_device_destroy(struct wl_listener *listener, void *data)
{
	struct input *input = wl_container_of(listener, input, destroy);
	wl_list_remove(&input->link);
	wl_list_remove(&input->destroy.link);

	/* `struct keyboard` is derived and has some extra clean up to do */
	if (input->wlr_input_device->type == WLR_INPUT_DEVICE_KEYBOARD) {
		struct keyboard *keyboard = (struct keyboard *)input;
		wl_list_remove(&keyboard->key.link);
		wl_list_remove(&keyboard->modifier.link);
	}
	free(input);
}

static bool
is_touch_device(struct wlr_input_device *wlr_input_device)
{
	switch (wlr_input_device->type) {
	case WLR_INPUT_DEVICE_TOUCH:
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		return true;
	default:
		break;
	}
	return false;
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
	current_type = is_touch_device(wlr_input_device)
			? TOUCH_DEVICE : NON_TOUCH_DEVICE;

	struct libinput_category *device_category, *dc = NULL;
	wl_list_for_each(device_category, &rc.libinput_categories, link) {
		if (device_category->name) {
			if (!strcasecmp(wlr_input_device->name,
					device_category->name)) {
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
		libinput_device_config_tap_set_button_map(libinput_dev,
			dc->tap_button_map);
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

static struct wlr_output *
output_by_name(struct server *server, const char *name)
{
	assert(name);
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (!strcasecmp(output->wlr_output->name, name)) {
			return output->wlr_output;
		}
	}
	return NULL;
}

static struct input *
new_pointer(struct seat *seat, struct wlr_input_device *dev)
{
	struct input *input = znew(*input);
	input->wlr_input_device = dev;

	if (wlr_input_device_is_libinput(dev)) {
		configure_libinput(dev);
	}
	wlr_cursor_attach_input_device(seat->cursor, dev);

	/* In support of running with WLR_WL_OUTPUTS set to >=2 */
	if (dev->type == WLR_INPUT_DEVICE_POINTER) {
		struct wlr_output *output = NULL;
		struct wlr_pointer *pointer = wlr_pointer_from_input_device(dev);
		wlr_log(WLR_INFO, "map pointer to output %s\n", pointer->output_name);
		if (pointer->output_name) {
			output = output_by_name(seat->server, pointer->output_name);
		}
		wlr_cursor_map_input_to_output(seat->cursor, dev, output);
		wlr_cursor_map_input_to_region(seat->cursor, dev, NULL);
	}
	return input;
}

static struct input *
new_keyboard(struct seat *seat, struct wlr_input_device *device, bool virtual)
{
	struct wlr_keyboard *kb = wlr_keyboard_from_input_device(device);

	struct keyboard *keyboard = znew(*keyboard);
	keyboard->base.wlr_input_device = device;
	keyboard->wlr_keyboard = kb;
	keyboard->is_virtual = virtual;

	wlr_keyboard_set_keymap(kb, seat->keyboard_group->keyboard.keymap);

	if (!virtual) {
		wlr_keyboard_group_add_keyboard(seat->keyboard_group, kb);
	}

	keyboard->key.notify = keyboard_key_notify;
	wl_signal_add(&kb->events.key, &keyboard->key);
	keyboard->modifier.notify = keyboard_modifiers_notify;
	wl_signal_add(&kb->events.modifiers, &keyboard->modifier);

	wlr_seat_set_keyboard(seat->seat, kb);

	return (struct input *)keyboard;
}

static struct input *
new_touch(struct seat *seat, struct wlr_input_device *dev)
{
	struct input *input = znew(*input);
	input->wlr_input_device = dev;

	if (wlr_input_device_is_libinput(dev)) {
		configure_libinput(dev);
	}
	wlr_cursor_attach_input_device(seat->cursor, dev);

	/* In support of running with WLR_WL_OUTPUTS set to >=2 */
	if (dev->type == WLR_INPUT_DEVICE_TOUCH) {
		struct wlr_output *output = NULL;
		struct wlr_touch *touch = wlr_touch_from_input_device(dev);
		wlr_log(WLR_INFO, "map touch to output %s\n", touch->output_name);
		if (touch->output_name) {
			output = output_by_name(seat->server, touch->output_name);
		}
		wlr_cursor_map_input_to_output(seat->cursor, dev, output);
		wlr_cursor_map_input_to_region(seat->cursor, dev, NULL);
	}
	return input;
}

static void
seat_update_capabilities(struct seat *seat)
{
	struct input *input = NULL;
	uint32_t caps = 0;

	wl_list_for_each(input, &seat->inputs, link) {
		switch (input->wlr_input_device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			caps |= WL_SEAT_CAPABILITY_KEYBOARD;
			break;
		case WLR_INPUT_DEVICE_POINTER:
			caps |= WL_SEAT_CAPABILITY_POINTER;
			break;
		case WLR_INPUT_DEVICE_TOUCH:
			caps |= WL_SEAT_CAPABILITY_TOUCH;
			break;
		default:
			break;
		}
	}
	wlr_seat_set_capabilities(seat->seat, caps);
}

static void
seat_add_device(struct seat *seat, struct input *input)
{
	input->seat = seat;
	input->destroy.notify = input_device_destroy;
	wl_signal_add(&input->wlr_input_device->events.destroy, &input->destroy);
	wl_list_insert(&seat->inputs, &input->link);

	seat_update_capabilities(seat);
}

static void
new_input_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, new_input);
	struct wlr_input_device *device = data;
	struct input *input = NULL;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		input = new_keyboard(seat, device, false);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		input = new_pointer(seat, device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		input = new_touch(seat, device);
		break;
	default:
		wlr_log(WLR_INFO, "unsupported input device");
		return;
	}

	seat_add_device(seat, input);
}

static void
destroy_idle_inhibitor(struct wl_listener *listener, void *data)
{
	struct idle_inhibitor *idle_inhibitor = wl_container_of(listener,
		idle_inhibitor, destroy);
	struct seat *seat = idle_inhibitor->seat;
	wl_list_remove(&idle_inhibitor->destroy.link);
	wlr_idle_set_enabled(seat->wlr_idle, seat->seat, wl_list_length(
			&seat->wlr_idle_inhibit_manager->inhibitors) <= 1);
	free(idle_inhibitor);
}

static void
new_idle_inhibitor(struct wl_listener *listener, void *data)
{
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;
	struct seat *seat = wl_container_of(listener, seat,
		idle_inhibitor_create);

	struct idle_inhibitor *inhibitor = znew(*inhibitor);
	inhibitor->seat = seat;
	inhibitor->wlr_inhibitor = wlr_inhibitor;
	inhibitor->destroy.notify = destroy_idle_inhibitor;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->destroy);
	wlr_idle_set_enabled(seat->wlr_idle, seat->seat, 0);
}

static void
new_virtual_pointer(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, virtual_pointer_new);
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;
	struct wlr_input_device *device = &pointer->pointer.base;

	struct input *input = new_pointer(seat, device);
	device->data = input;
	seat_add_device(seat, input);
	if (event->suggested_output) {
		wlr_cursor_map_input_to_output(seat->cursor, device,
			event->suggested_output);
	}
}

static void
new_virtual_keyboard(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, virtual_keyboard_new);
	struct wlr_virtual_keyboard_v1 *virtual_keyboard = data;
	struct wlr_input_device *device = &virtual_keyboard->keyboard.base;

	struct input *input = new_keyboard(seat, device, true);
	device->data = input;
	seat_add_device(seat, input);
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

	seat->wlr_idle = wlr_idle_create(server->wl_display);
	seat->wlr_idle_inhibit_manager = wlr_idle_inhibit_v1_create(
		server->wl_display);
	wl_signal_add(&seat->wlr_idle_inhibit_manager->events.new_inhibitor,
		&seat->idle_inhibitor_create);
	seat->idle_inhibitor_create.notify = new_idle_inhibitor;

	seat->virtual_pointer = wlr_virtual_pointer_manager_v1_create(
		server->wl_display);
	wl_signal_add(&seat->virtual_pointer->events.new_virtual_pointer,
		&seat->virtual_pointer_new);
	seat->virtual_pointer_new.notify = new_virtual_pointer;

	seat->virtual_keyboard = wlr_virtual_keyboard_manager_v1_create(
		server->wl_display);
	wl_signal_add(&seat->virtual_keyboard->events.new_virtual_keyboard,
		&seat->virtual_keyboard_new);
	seat->virtual_keyboard_new.notify = new_virtual_keyboard;

	seat->cursor = wlr_cursor_create();
	if (!seat->cursor) {
		wlr_log(WLR_ERROR, "unable to create cursor");
		exit(EXIT_FAILURE);
	}
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	keyboard_init(seat);
	cursor_init(seat);
	touch_init(seat);
}

void
seat_finish(struct server *server)
{
	struct seat *seat = &server->seat;
	wl_list_remove(&seat->new_input.link);

	struct input *input, *next;
	wl_list_for_each_safe(input, next, &seat->inputs, link) {
		input_device_destroy(&input->destroy, NULL);
	}

	keyboard_finish(seat);
	/*
	 * Caution - touch_finish() unregisters event listeners from
	 * seat->cursor and must come before cursor_finish(), otherwise
	 * a use-after-free occurs.
	 */
	touch_finish(seat);
	cursor_finish(seat);
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

	/*
	 * Key events associated with keybindings (both pressed and released)
	 * are not sent to clients. When changing surface-focus it is therefore
	 * important not to send the keycodes of _all_ pressed keys, but only
	 * those that were actually _sent_ to clients (that is, those that were
	 * not bound).
	 */
	uint32_t *pressed_sent_keycodes = key_state_pressed_sent_keycodes();
	int nr_pressed_sent_keycodes = key_state_nr_pressed_sent_keycodes();

	wlr_seat_keyboard_notify_enter(seat->seat, surface,
		pressed_sent_keycodes, nr_pressed_sent_keycodes, &kb->modifiers);

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
		return;
	}
	seat_focus_surface(seat, layer->surface);
	if (layer->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
		seat->focused_layer = layer;
	}
}

static void
pressed_surface_destroy(struct wl_listener *listener, void *data)
{
	struct wlr_surface *surface = data;
	struct seat *seat = wl_container_of(listener, seat,
		pressed_surface_destroy);

	assert(surface == seat->pressed.surface);
	seat_reset_pressed(seat);
}

void
seat_set_pressed(struct seat *seat, struct view *view,
	struct wlr_scene_node *node, struct wlr_surface *surface,
	struct wlr_surface *toplevel, uint32_t resize_edges)
{
	assert(view || surface);
	seat_reset_pressed(seat);

	seat->pressed.view = view;
	seat->pressed.node = node;
	seat->pressed.surface = surface;
	seat->pressed.toplevel = toplevel;
	seat->pressed.resize_edges = resize_edges;

	if (surface) {
		seat->pressed_surface_destroy.notify = pressed_surface_destroy;
		wl_signal_add(&surface->events.destroy,
			&seat->pressed_surface_destroy);
	}
}

void
seat_reset_pressed(struct seat *seat)
{
	if (seat->pressed.surface) {
		wl_list_remove(&seat->pressed_surface_destroy.link);
	}

	seat->pressed.view = NULL;
	seat->pressed.node = NULL;
	seat->pressed.surface = NULL;
	seat->pressed.toplevel = NULL;
	seat->pressed.resize_edges = 0;
}
