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
#include "input/ime.h"
#include "input/tablet.h"
#include "input/tablet-pad.h"
#include "input/input.h"
#include "input/keyboard.h"
#include "input/key-state.h"
#include "labwc.h"
#include "view.h"

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
		keyboard_cancel_keybind_repeat(keyboard);
	}
	free(input);
}

static enum lab_libinput_device_type
device_type_from_wlr_device(struct wlr_input_device *wlr_input_device)
{
	switch (wlr_input_device->type) {
	case WLR_INPUT_DEVICE_TOUCH:
	case WLR_INPUT_DEVICE_TABLET:
		return LAB_LIBINPUT_DEVICE_TOUCH;
	default:
		break;
	}

	if (wlr_input_device->type == WLR_INPUT_DEVICE_POINTER &&
			wlr_input_device_is_libinput(wlr_input_device)) {
		struct libinput_device *libinput_device =
			wlr_libinput_get_device_handle(wlr_input_device);

		if (libinput_device_config_tap_get_finger_count(libinput_device) > 0) {
			return LAB_LIBINPUT_DEVICE_TOUCHPAD;
		}
	}

	return LAB_LIBINPUT_DEVICE_NON_TOUCH;
}

/*
 * Get applicable profile (category) by matching first by name and secondly be
 * type (e.g. 'touch' and 'non-touch'). If not suitable match is found based on
 * those two criteria we fallback on 'default'.
 */
static struct libinput_category *
get_category(struct wlr_input_device *device)
{
	/* By name */
	struct libinput_category *category;
	wl_list_for_each_reverse(category, &rc.libinput_categories, link) {
		if (category->name) {
			if (!strcasecmp(device->name, category->name)) {
				return category;
			}
		}
	}

	/* By type */
	enum lab_libinput_device_type type = device_type_from_wlr_device(device);
	wl_list_for_each_reverse(category, &rc.libinput_categories, link) {
		if (category->type == type) {
			return category;
		}
	}

	/* Use default profile as a fallback */
	return libinput_category_get_default();
}

static void
configure_libinput(struct wlr_input_device *wlr_input_device)
{
	/*
	 * TODO: We do not check any return values for the various
	 *       libinput_device_config_*_set_*() calls. It would
	 *       be nice if we could inform the users via log file
	 *       that some libinput setting could not be applied.
	 *
	 * TODO: We are currently using int32_t with -1 as default
	 *       to describe the not-configured state. This is not
	 *       really optimal as we can't properly deal with
	 *       enum values that are 0. After some discussion via
	 *       IRC the best way forward seem to be to use a
	 *       uint32_t instead and UINT32_MAX as indicator for
	 *       a not-configured state. This allows to properly
	 *       test the enum being a member of a bitset via
	 *       mask & value == value. All libinput enums are
	 *       way below UINT32_MAX.
	 */

	if (!wlr_input_device) {
		wlr_log(WLR_ERROR, "no wlr_input_device");
		return;
	}
	struct input *input = wlr_input_device->data;

	/* Set scroll factor to 1.0 for Wayland/X11 backends or virtual pointers */
	if (!wlr_input_device_is_libinput(wlr_input_device)) {
		input->scroll_factor = 1.0;
		return;
	}

	struct libinput_device *libinput_dev =
		wlr_libinput_get_device_handle(wlr_input_device);
	if (!libinput_dev) {
		wlr_log(WLR_ERROR, "no libinput_dev");
		return;
	}

	struct libinput_category *dc = get_category(wlr_input_device);

	/*
	 * The above logic should have always matched SOME category
	 * (the default category if none other took precedence)
	 */
	assert(dc);

	if (libinput_device_config_tap_get_finger_count(libinput_dev) <= 0) {
		wlr_log(WLR_INFO, "tap unavailable");
	} else {
		wlr_log(WLR_INFO, "tap configured");
		libinput_device_config_tap_set_enabled(libinput_dev, dc->tap);
		libinput_device_config_tap_set_button_map(libinput_dev,
			dc->tap_button_map);
	}

	if (libinput_device_config_tap_get_finger_count(libinput_dev) <= 0
			|| dc->tap_and_drag < 0) {
		wlr_log(WLR_INFO, "tap-and-drag not configured");
	} else {
		wlr_log(WLR_INFO, "tap-and-drag configured");
		libinput_device_config_tap_set_drag_enabled(
			libinput_dev, dc->tap_and_drag);
	}

	if (libinput_device_config_tap_get_finger_count(libinput_dev) <= 0
			|| dc->drag_lock < 0) {
		wlr_log(WLR_INFO, "drag lock not configured");
	} else {
		wlr_log(WLR_INFO, "drag lock configured");
		libinput_device_config_tap_set_drag_lock_enabled(
			libinput_dev, dc->drag_lock);
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
		if (dc->pointer_speed >= -1) {
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

	if ((dc->click_method != LIBINPUT_CONFIG_CLICK_METHOD_NONE
			&& (libinput_device_config_click_get_methods(libinput_dev)
				& dc->click_method) == 0)
			|| dc->click_method < 0) {
		wlr_log(WLR_INFO, "click method not configured");
	} else {
		wlr_log(WLR_INFO, "click method configured");

		/*
		 * Note, the documentation claims that:
		 * > [...] The device may require changing to a neutral state
		 * > first before activating the new method.
		 *
		 * However, just setting the method seems to work without
		 * issues.
		 */

		libinput_device_config_click_set_method(libinput_dev, dc->click_method);
	}

	if ((dc->send_events_mode != LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
			&& (libinput_device_config_send_events_get_modes(libinput_dev)
				& dc->send_events_mode) == 0)
			|| dc->send_events_mode < 0) {
		wlr_log(WLR_INFO, "send events mode not configured");
	} else {
		wlr_log(WLR_INFO, "send events mode configured");
		libinput_device_config_send_events_set_mode(libinput_dev, dc->send_events_mode);
	}

	/* Non-zero if the device can be calibrated, zero otherwise. */
	if (libinput_device_config_calibration_has_matrix(libinput_dev) == 0
			|| !dc->have_calibration_matrix) {
		wlr_log(WLR_INFO, "calibration matrix not configured");
	} else {
		wlr_log(WLR_INFO, "calibration matrix configured");
		libinput_device_config_calibration_set_matrix(libinput_dev, dc->calibration_matrix);
	}

	wlr_log(WLR_INFO, "scroll factor configured");
	input->scroll_factor = dc->scroll_factor;
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

static void
map_input_to_output(struct seat *seat, struct wlr_input_device *dev, char *output_name)
{
	struct wlr_output *output = NULL;
	if (output_name) {
		output = output_by_name(seat->server, output_name);
	}
	wlr_cursor_map_input_to_output(seat->cursor, dev, output);
	wlr_cursor_map_input_to_region(seat->cursor, dev, NULL);
}

static void
map_pointer_to_output(struct seat *seat, struct wlr_input_device *dev)
{
	struct wlr_pointer *pointer = wlr_pointer_from_input_device(dev);
	wlr_log(WLR_INFO, "map pointer to output %s\n", pointer->output_name);
	map_input_to_output(seat, dev, pointer->output_name);
}

static struct input *
new_pointer(struct seat *seat, struct wlr_input_device *dev)
{
	struct input *input = znew(*input);
	input->wlr_input_device = dev;
	dev->data = input;
	configure_libinput(dev);
	wlr_cursor_attach_input_device(seat->cursor, dev);

	/* In support of running with WLR_WL_OUTPUTS set to >=2 */
	if (dev->type == WLR_INPUT_DEVICE_POINTER) {
		map_pointer_to_output(seat, dev);
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

	if (!seat->keyboard_group->keyboard.keymap) {
		wlr_log(WLR_ERROR, "cannot set keymap");
		exit(EXIT_FAILURE);
	}

	wlr_keyboard_set_keymap(kb, seat->keyboard_group->keyboard.keymap);

	/*
	 * This needs to be before wlr_keyboard_group_add_keyboard().
	 * For some reason, wlroots takes the modifier state from the
	 * new keyboard and syncs it to the others in the group, rather
	 * than the other way around.
	 */
	keyboard_set_numlock(kb);

	if (!virtual) {
		wlr_keyboard_group_add_keyboard(seat->keyboard_group, kb);
	}

	keyboard_setup_handlers(keyboard);

	wlr_seat_set_keyboard(seat->seat, kb);

	return (struct input *)keyboard;
}

static void
map_touch_to_output(struct seat *seat, struct wlr_input_device *dev)
{
	struct wlr_touch *touch = wlr_touch_from_input_device(dev);

	char *touch_config_output_name = NULL;
	struct touch_config_entry *config_entry =
		touch_find_config_for_device(touch->base.name);
	if (config_entry) {
		touch_config_output_name = config_entry->output_name;
	}

	char *output_name = touch->output_name ? touch->output_name : touch_config_output_name;
	wlr_log(WLR_INFO, "map touch to output %s\n", output_name ? output_name : "unknown");
	map_input_to_output(seat, dev, output_name);
}

static struct input *
new_touch(struct seat *seat, struct wlr_input_device *dev)
{
	struct input *input = znew(*input);
	input->wlr_input_device = dev;
	dev->data = input;
	configure_libinput(dev);
	wlr_cursor_attach_input_device(seat->cursor, dev);
	/* In support of running with WLR_WL_OUTPUTS set to >=2 */
	map_touch_to_output(seat, dev);

	return input;
}

static struct input *
new_tablet(struct seat *seat, struct wlr_input_device *dev)
{
	struct input *input = znew(*input);
	input->wlr_input_device = dev;
	tablet_create(seat, dev);
	wlr_cursor_attach_input_device(seat->cursor, dev);
	wlr_log(WLR_INFO, "map tablet to output %s\n", rc.tablet.output_name);
	map_input_to_output(seat, dev, rc.tablet.output_name);

	return input;
}

static struct input *
new_tablet_pad(struct seat *seat, struct wlr_input_device *dev)
{
	struct input *input = znew(*input);
	input->wlr_input_device = dev;
	tablet_pad_create(seat, dev);

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
		case WLR_INPUT_DEVICE_TABLET:
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
	case WLR_INPUT_DEVICE_TABLET:
		input = new_tablet(seat, device);
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		input = new_tablet_pad(seat, device);
		break;
	default:
		wlr_log(WLR_INFO, "unsupported input device");
		return;
	}

	seat_add_device(seat, input);
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

static void
focus_change_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, focus_change);
	struct wlr_seat_keyboard_focus_change_event *event = data;
	struct server *server = seat->server;
	struct wlr_surface *surface = event->new_surface;
	struct view *view = surface ? view_from_wlr_surface(surface) : NULL;

	/*
	 * Prevent focus switch to non-view surface (e.g. layer-shell
	 * or xwayland-unmanaged) from updating view state
	 */
	if (surface && !view) {
		return;
	}

	if (view != server->active_view) {
		if (server->active_view) {
			view_set_activated(server->active_view, false);
		}
		if (view) {
			view_set_activated(view, true);
			tablet_pad_enter_surface(seat, surface);
		}
		server->active_view = view;
	}
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

	wl_list_init(&seat->touch_points);
	wl_list_init(&seat->constraint_commit.link);
	wl_list_init(&seat->inputs);
	seat->new_input.notify = new_input_notify;
	wl_signal_add(&server->backend->events.new_input, &seat->new_input);

	seat->focus_change.notify = focus_change_notify;
	wl_signal_add(&seat->seat->keyboard_state.events.focus_change,
		&seat->focus_change);

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

	seat->input_method_relay = input_method_relay_create(seat);

	seat->xcursor_manager = NULL;
	seat->cursor = wlr_cursor_create();
	if (!seat->cursor) {
		wlr_log(WLR_ERROR, "unable to create cursor");
		exit(EXIT_FAILURE);
	}
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	wl_list_init(&seat->tablets);
	wl_list_init(&seat->tablet_tools);
	wl_list_init(&seat->tablet_pads);

	input_handlers_init(seat);
}

void
seat_finish(struct server *server)
{
	struct seat *seat = &server->seat;
	wl_list_remove(&seat->new_input.link);
	wl_list_remove(&seat->focus_change.link);

	struct input *input, *next;
	wl_list_for_each_safe(input, next, &seat->inputs, link) {
		input_device_destroy(&input->destroy, NULL);
	}

	input_handlers_finish(seat);
	input_method_relay_finish(seat->input_method_relay);
}

static void
configure_keyboard(struct seat *seat, struct input *input)
{
	struct wlr_input_device *device = input->wlr_input_device;
	assert(device->type == WLR_INPUT_DEVICE_KEYBOARD);
	struct keyboard *keyboard = (struct keyboard *)input;
	struct wlr_keyboard *kb = wlr_keyboard_from_input_device(device);
	keyboard_configure(seat, kb, keyboard->is_virtual);
}

/* This is called on SIGHUP (generally in response to labwc --reconfigure */
void
seat_reconfigure(struct server *server)
{
	struct seat *seat = &server->seat;
	struct input *input;
	cursor_reload(seat);
	overlay_reconfigure(seat);
	keyboard_reset_current_keybind();
	wl_list_for_each(input, &seat->inputs, link) {
		switch (input->wlr_input_device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			configure_keyboard(seat, input);
			break;
		case WLR_INPUT_DEVICE_POINTER:
			configure_libinput(input->wlr_input_device);
			map_pointer_to_output(seat, input->wlr_input_device);
			break;
		case WLR_INPUT_DEVICE_TOUCH:
			configure_libinput(input->wlr_input_device);
			map_touch_to_output(seat, input->wlr_input_device);
			break;
		case WLR_INPUT_DEVICE_TABLET:
			map_input_to_output(seat, input->wlr_input_device, rc.tablet.output_name);
			break;
		default:
			break;
		}
	}
}

static void
seat_focus(struct seat *seat, struct wlr_surface *surface, bool is_lock_surface)
{
	/*
	 * Respect session lock. This check is critical, DO NOT REMOVE.
	 * It should also come before the !surface condition, or the
	 * lock screen may lose focus and become impossible to unlock.
	 */
	struct server *server = seat->server;
	if (server->session_lock_manager->locked && !is_lock_surface) {
		return;
	}

	if (!surface) {
		wlr_seat_keyboard_notify_clear_focus(seat->seat);
		input_method_relay_set_focus(seat->input_method_relay, NULL);
		return;
	}

	if (!wlr_seat_get_keyboard(seat->seat)) {
		/*
		 * wlr_seat_keyboard_notify_enter() sends wl_keyboard.modifiers,
		 * but it may crash some apps (e.g. Chromium) if
		 * wl_keyboard.keymap is not sent beforehand.
		 */
		wlr_seat_set_keyboard(seat->seat, &seat->keyboard_group->keyboard);
	}

	/*
	 * Key events associated with keybindings (both pressed and released)
	 * are not sent to clients. When changing surface-focus it is therefore
	 * important not to send the keycodes of _all_ pressed keys, but only
	 * those that were actually _sent_ to clients (that is, those that were
	 * not bound).
	 */
	uint32_t *pressed_sent_keycodes = key_state_pressed_sent_keycodes();
	int nr_pressed_sent_keycodes = key_state_nr_pressed_sent_keycodes();

	struct wlr_keyboard *kb = &seat->keyboard_group->keyboard;
	wlr_seat_keyboard_notify_enter(seat->seat, surface,
		pressed_sent_keycodes, nr_pressed_sent_keycodes, &kb->modifiers);

	input_method_relay_set_focus(seat->input_method_relay, surface);

	struct wlr_pointer_constraint_v1 *constraint =
		wlr_pointer_constraints_v1_constraint_for_surface(server->constraints,
			surface, seat->seat);
	constrain_cursor(server, constraint);
}

void
seat_focus_surface(struct seat *seat, struct wlr_surface *surface)
{
	/* Respect layer-shell exclusive keyboard-interactivity. */
	if (seat->focused_layer && seat->focused_layer->current.keyboard_interactive
			== ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
		return;
	}
	seat_focus(seat, surface, /*is_lock_surface*/ false);
}

void
seat_focus_lock_surface(struct seat *seat, struct wlr_surface *surface)
{
	seat_focus(seat, surface, /*is_lock_surface*/ true);
}

void
seat_set_focus_layer(struct seat *seat, struct wlr_layer_surface_v1 *layer)
{
	if (!layer) {
		seat->focused_layer = NULL;
		desktop_focus_topmost_view(seat->server);
		return;
	}
	seat_focus(seat, layer->surface, /*is_lock_surface*/ false);
	seat->focused_layer = layer;
}

static void
pressed_surface_destroy(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat,
		pressed_surface_destroy);

	/*
	 * Using data directly prevents 'unused variable'
	 * warning when compiling without asserts
	 */
	assert(data == seat->pressed.surface);

	seat_reset_pressed(seat);
}

void
seat_set_pressed(struct seat *seat, struct cursor_context *ctx)
{
	assert(ctx);
	assert(ctx->view || ctx->surface);
	seat_reset_pressed(seat);

	seat->pressed = *ctx;

	if (ctx->surface) {
		seat->pressed_surface_destroy.notify = pressed_surface_destroy;
		wl_signal_add(&ctx->surface->events.destroy,
			&seat->pressed_surface_destroy);
	}
}

void
seat_reset_pressed(struct seat *seat)
{
	if (seat->pressed.surface) {
		wl_list_remove(&seat->pressed_surface_destroy.link);
	}
	seat->pressed = (struct cursor_context){0};
}

void
seat_output_layout_changed(struct seat *seat)
{
	struct input *input = NULL;
	wl_list_for_each(input, &seat->inputs, link) {
		switch (input->wlr_input_device->type) {
		case WLR_INPUT_DEVICE_POINTER:
			map_pointer_to_output(seat, input->wlr_input_device);
			break;
		case WLR_INPUT_DEVICE_TOUCH:
			map_touch_to_output(seat, input->wlr_input_device);
			break;
		case WLR_INPUT_DEVICE_TABLET:
			map_input_to_output(seat, input->wlr_input_device, rc.tablet.output_name);
			break;
		default:
			break;
		}
	}
}
