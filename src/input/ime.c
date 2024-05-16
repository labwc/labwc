// SPDX-License-Identifier: GPL-2.0-only
/* Based on Sway (https://github.com/swaywm/sway) */

#include <assert.h>
#include "common/mem.h"
#include "input/ime.h"
#include "node.h"
#include "view.h"

#define SAME_CLIENT(wlr_obj1, wlr_obj2) \
	(wl_resource_get_client((wlr_obj1)->resource) \
	== wl_resource_get_client((wlr_obj2)->resource))

static bool
is_keyboard_emulated_by_input_method(struct wlr_keyboard *keyboard,
		struct wlr_input_method_v2 *input_method)
{
	if (!keyboard || !input_method) {
		return false;
	}

	struct wlr_virtual_keyboard_v1 *virtual_keyboard =
		wlr_input_device_get_virtual_keyboard(&keyboard->base);

	return virtual_keyboard && SAME_CLIENT(virtual_keyboard, input_method);
}

/*
 * Get keyboard grab of the seat from keyboard if we should forward events
 * to it.
 */
static struct wlr_input_method_keyboard_grab_v2 *
get_keyboard_grab(struct keyboard *keyboard)
{
	struct wlr_input_method_v2 *input_method =
		keyboard->base.seat->input_method_relay->input_method;
	if (!input_method || !input_method->keyboard_grab) {
		return NULL;
	}

	/*
	 * Input-methods often use virtual keyboard to send raw key signals
	 * instead of sending encoded text via set_preedit_string and
	 * commit_string. We should not forward those key events back to the
	 * input-method so key events don't loop between the compositor and
	 * the input-method.
	 */
	if (is_keyboard_emulated_by_input_method(
			keyboard->wlr_keyboard, input_method)) {
		return NULL;
	}

	return input_method->keyboard_grab;
}

bool
input_method_keyboard_grab_forward_modifiers(struct keyboard *keyboard)
{
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab =
		get_keyboard_grab(keyboard);
	if (keyboard_grab) {
		wlr_input_method_keyboard_grab_v2_set_keyboard(keyboard_grab,
			keyboard->wlr_keyboard);
		wlr_input_method_keyboard_grab_v2_send_modifiers(keyboard_grab,
			&keyboard->wlr_keyboard->modifiers);
		return true;
	} else {
		return false;
	}
}

bool
input_method_keyboard_grab_forward_key(struct keyboard *keyboard,
		struct wlr_keyboard_key_event *event)
{
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab =
		get_keyboard_grab(keyboard);
	if (keyboard_grab) {
		wlr_input_method_keyboard_grab_v2_set_keyboard(keyboard_grab,
			keyboard->wlr_keyboard);
		wlr_input_method_keyboard_grab_v2_send_key(keyboard_grab,
			event->time_msec, event->keycode, event->state);
		return true;
	} else {
		return false;
	}
}

/*
 * update_text_inputs_focused_surface() should be called beforehand to set
 * right text-inputs to choose from.
 */
static struct text_input *
get_active_text_input(struct input_method_relay *relay)
{
	if (!relay->input_method) {
		return NULL;
	}
	struct text_input *text_input;
	wl_list_for_each(text_input, &relay->text_inputs, link) {
		if (text_input->input->focused_surface
				&& text_input->input->current_enabled) {
			return text_input;
		}
	}
	return NULL;
}

/*
 * Updates active text-input and activates/deactivates the input-method if the
 * value is changed.
 */
static void
update_active_text_input(struct input_method_relay *relay)
{
	struct text_input *active_text_input = get_active_text_input(relay);

	if (relay->input_method && relay->active_text_input
			!= active_text_input) {
		if (active_text_input) {
			wlr_input_method_v2_send_activate(relay->input_method);
		} else {
			wlr_input_method_v2_send_deactivate(relay->input_method);
		}
		wlr_input_method_v2_send_done(relay->input_method);
	}

	relay->active_text_input = active_text_input;
}

/*
 * Updates focused surface of text-inputs and sends enter/leave events to
 * the text-inputs whose focused surface is changed.
 * When input-method is present, text-inputs whose client is the same as the
 * relay's focused surface also have that focused surface. Clients can then
 * send enable request on a text-input which has the focused surface to make
 * the text-input active and start communicating with input-method.
 */
static void
update_text_inputs_focused_surface(struct input_method_relay *relay)
{
	struct text_input *text_input;
	wl_list_for_each(text_input, &relay->text_inputs, link) {
		struct wlr_text_input_v3 *input = text_input->input;

		struct wlr_surface *new_focused_surface;
		if (relay->input_method && relay->focused_surface
				&& SAME_CLIENT(input, relay->focused_surface)) {
			new_focused_surface = relay->focused_surface;
		} else {
			new_focused_surface = NULL;
		}

		if (input->focused_surface == new_focused_surface) {
			continue;
		}
		if (input->focused_surface) {
			wlr_text_input_v3_send_leave(input);
		}
		if (new_focused_surface) {
			wlr_text_input_v3_send_enter(input, new_focused_surface);
		}
	}
}

static void
update_popup_position(struct input_method_popup *popup)
{
	struct input_method_relay *relay = popup->relay;
	struct server *server = relay->seat->server;
	struct text_input *text_input = relay->active_text_input;

	if (!text_input || !relay->focused_surface
			|| !popup->popup_surface->surface->mapped) {
		return;
	}

	struct wlr_box cursor_rect;
	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_try_from_wlr_surface(relay->focused_surface);
	struct wlr_layer_surface_v1 *layer_surface =
		wlr_layer_surface_v1_try_from_wlr_surface(relay->focused_surface);

	if ((text_input->input->current.features
			& WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE)
			&& (xdg_surface || layer_surface)) {
		cursor_rect = text_input->input->current.cursor_rectangle;

		/*
		 * wlr_surface->data is:
		 * - for XDG surfaces: view->scene_node
		 * - for layer surfaces: lab_layer_surface->scene_layer_surface->tree
		 * - for layer popups: lab_layer_popup->scene_tree
		 */
		struct wlr_scene_tree *tree = relay->focused_surface->data;
		int lx, ly;
		wlr_scene_node_coords(&tree->node, &lx, &ly);
		cursor_rect.x += lx;
		cursor_rect.y += ly;

		if (xdg_surface) {
			/* Take into account invisible xdg-shell CSD borders */
			struct wlr_box geo;
			wlr_xdg_surface_get_geometry(xdg_surface, &geo);
			cursor_rect.x -= geo.x;
			cursor_rect.y -= geo.y;
		}
	} else {
		cursor_rect = (struct wlr_box){0};
	}

	struct output *output =
		output_nearest_to(server, cursor_rect.x, cursor_rect.y);
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR,
			"Cannot position IME popup (unusable output)");
		return;
	}
	struct wlr_box output_box;
	wlr_output_layout_get_box(
		server->output_layout, output->wlr_output, &output_box);

	/* Use xdg-positioner utilities to position popup */
	struct wlr_xdg_positioner_rules rules = {
		.anchor_rect = cursor_rect,
		.anchor = XDG_POSITIONER_ANCHOR_BOTTOM_LEFT,
		.gravity = XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
		.size = {
			.width = popup->popup_surface->surface->current.width,
			.height = popup->popup_surface->surface->current.height,
		},
		.constraint_adjustment =
			XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y
			| XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X,
	};

	struct wlr_box popup_box;
	wlr_xdg_positioner_rules_get_geometry(&rules, &popup_box);
	wlr_xdg_positioner_rules_unconstrain_box(&rules, &output_box, &popup_box);

	wlr_scene_node_set_position(
		&popup->tree->node, popup_box.x, popup_box.y);
	/* Make sure IME popups are always on top, above layer-shell surfaces */
	wlr_scene_node_raise_to_top(&relay->popup_tree->node);

	wlr_input_popup_surface_v2_send_text_input_rectangle(
		popup->popup_surface, &(struct wlr_box){
			.x = cursor_rect.x - popup_box.x,
			.y = cursor_rect.y - popup_box.y,
			.width = cursor_rect.width,
			.height = cursor_rect.height,
		});
}

static void
update_popups_position(struct input_method_relay *relay)
{
	struct input_method_popup *popup;
	wl_list_for_each(popup, &relay->popups, link) {
		update_popup_position(popup);
	}
}

static void
handle_input_method_commit(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay =
		wl_container_of(listener, relay, input_method_commit);
	struct wlr_input_method_v2 *input_method = data;
	assert(relay->input_method == input_method);

	struct text_input *text_input = relay->active_text_input;
	if (!text_input) {
		return;
	}

	if (input_method->current.preedit.text) {
		wlr_text_input_v3_send_preedit_string(
			text_input->input,
			input_method->current.preedit.text,
			input_method->current.preedit.cursor_begin,
			input_method->current.preedit.cursor_end);
	}
	if (input_method->current.commit_text) {
		wlr_text_input_v3_send_commit_string(
			text_input->input,
			input_method->current.commit_text);
	}
	if (input_method->current.delete.before_length
			|| input_method->current.delete.after_length) {
		wlr_text_input_v3_send_delete_surrounding_text(
			text_input->input,
			input_method->current.delete.before_length,
			input_method->current.delete.after_length);
	}
	wlr_text_input_v3_send_done(text_input->input);
}

static void
handle_keyboard_grab_destroy(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay =
		wl_container_of(listener, relay, keyboard_grab_destroy);
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = data;
	wl_list_remove(&relay->keyboard_grab_destroy.link);

	if (keyboard_grab->keyboard) {
		/* Send modifier state to original client */
		wlr_seat_keyboard_notify_modifiers(
			keyboard_grab->input_method->seat,
			&keyboard_grab->keyboard->modifiers);
	}
}

static void
handle_input_method_grab_keyboard(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay = wl_container_of(listener, relay,
		input_method_grab_keyboard);
	struct wlr_input_method_keyboard_grab_v2 *keyboard_grab = data;

	struct wlr_keyboard *active_keyboard =
		wlr_seat_get_keyboard(relay->seat->seat);

	if (!is_keyboard_emulated_by_input_method(
			active_keyboard, relay->input_method)) {
		/* Send modifier state to grab */
		wlr_input_method_keyboard_grab_v2_set_keyboard(
			keyboard_grab, active_keyboard);
	}

	relay->keyboard_grab_destroy.notify = handle_keyboard_grab_destroy;
	wl_signal_add(&keyboard_grab->events.destroy,
		&relay->keyboard_grab_destroy);
}

static void
handle_input_method_destroy(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay =
		wl_container_of(listener, relay, input_method_destroy);
	assert(relay->input_method == data);
	wl_list_remove(&relay->input_method_commit.link);
	wl_list_remove(&relay->input_method_grab_keyboard.link);
	wl_list_remove(&relay->input_method_destroy.link);
	relay->input_method = NULL;

	update_text_inputs_focused_surface(relay);
	update_active_text_input(relay);
}

static void
handle_popup_surface_destroy(struct wl_listener *listener, void *data)
{
	struct input_method_popup *popup =
		wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->link);
	free(popup);
}

static void
handle_popup_surface_commit(struct wl_listener *listener, void *data)
{
	struct input_method_popup *popup =
		wl_container_of(listener, popup, commit);
	update_popup_position(popup);
}

static void
handle_input_method_new_popup_surface(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay = wl_container_of(listener, relay,
		input_method_new_popup_surface);

	struct input_method_popup *popup = znew(*popup);
	popup->popup_surface = data;
	popup->relay = relay;

	popup->destroy.notify = handle_popup_surface_destroy;
	wl_signal_add(&popup->popup_surface->events.destroy, &popup->destroy);

	popup->commit.notify = handle_popup_surface_commit;
	wl_signal_add(&popup->popup_surface->surface->events.commit,
		&popup->commit);

	popup->tree = wlr_scene_subsurface_tree_create(
		relay->popup_tree, popup->popup_surface->surface);
	node_descriptor_create(&popup->tree->node, LAB_NODE_DESC_IME_POPUP, NULL);

	wl_list_insert(&relay->popups, &popup->link);
}

static void
handle_new_input_method(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay =
		wl_container_of(listener, relay, new_input_method);
	struct wlr_input_method_v2 *input_method = data;
	if (relay->seat->seat != input_method->seat) {
		return;
	}

	if (relay->input_method) {
		wlr_log(WLR_INFO,
			"Attempted to connect second input method to a seat");
		wlr_input_method_v2_send_unavailable(input_method);
		return;
	}

	relay->input_method = input_method;

	relay->input_method_commit.notify = handle_input_method_commit;
	wl_signal_add(&relay->input_method->events.commit,
		&relay->input_method_commit);

	relay->input_method_grab_keyboard.notify =
		handle_input_method_grab_keyboard;
	wl_signal_add(&relay->input_method->events.grab_keyboard,
		&relay->input_method_grab_keyboard);

	relay->input_method_destroy.notify = handle_input_method_destroy;
	wl_signal_add(&relay->input_method->events.destroy,
		&relay->input_method_destroy);

	relay->input_method_new_popup_surface.notify =
		handle_input_method_new_popup_surface;
	wl_signal_add(&relay->input_method->events.new_popup_surface,
		&relay->input_method_new_popup_surface);

	update_text_inputs_focused_surface(relay);
	update_active_text_input(relay);
}

/* Conveys state from active text-input to input-method */
static void
send_state_to_input_method(struct input_method_relay *relay)
{
	assert(relay->active_text_input && relay->input_method);

	struct wlr_input_method_v2 *input_method = relay->input_method;
	struct wlr_text_input_v3 *input = relay->active_text_input->input;

	/* TODO: only send each of those if they were modified */
	if (input->active_features
			& WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) {
		wlr_input_method_v2_send_surrounding_text(input_method,
			input->current.surrounding.text,
			input->current.surrounding.cursor,
			input->current.surrounding.anchor);
	}
	wlr_input_method_v2_send_text_change_cause(input_method,
		input->current.text_change_cause);
	if (input->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE) {
		wlr_input_method_v2_send_content_type(input_method,
			input->current.content_type.hint,
			input->current.content_type.purpose);
	}
	wlr_input_method_v2_send_done(input_method);
}

static void
handle_text_input_enable(struct wl_listener *listener, void *data)
{
	struct text_input *text_input =
		wl_container_of(listener, text_input, enable);
	struct input_method_relay *relay = text_input->relay;

	update_active_text_input(relay);
	if (relay->active_text_input == text_input) {
		update_popups_position(relay);
		send_state_to_input_method(relay);
	}
}

static void
handle_text_input_disable(struct wl_listener *listener, void *data)
{
	struct text_input *text_input =
		wl_container_of(listener, text_input, disable);
	/*
	 * When the focus is moved between surfaces from different clients and
	 * then the old client sends "disable" event, the relay ignores it and
	 * doesn't deactivate the input-method.
	 */
	update_active_text_input(text_input->relay);
}

static void
handle_text_input_commit(struct wl_listener *listener, void *data)
{
	struct text_input *text_input =
		wl_container_of(listener, text_input, commit);
	struct input_method_relay *relay = text_input->relay;

	if (relay->active_text_input == text_input) {
		update_popups_position(relay);
		send_state_to_input_method(relay);
	}
}

static void
handle_text_input_destroy(struct wl_listener *listener, void *data)
{
	struct text_input *text_input =
		wl_container_of(listener, text_input, destroy);
	wl_list_remove(&text_input->enable.link);
	wl_list_remove(&text_input->disable.link);
	wl_list_remove(&text_input->commit.link);
	wl_list_remove(&text_input->destroy.link);
	wl_list_remove(&text_input->link);
	update_active_text_input(text_input->relay);
	free(text_input);
}

static void
handle_new_text_input(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay =
		wl_container_of(listener, relay, new_text_input);
	struct wlr_text_input_v3 *wlr_text_input = data;
	if (relay->seat->seat != wlr_text_input->seat) {
		return;
	}

	struct text_input *text_input = znew(*text_input);
	text_input->input = wlr_text_input;
	text_input->relay = relay;
	wl_list_insert(&relay->text_inputs, &text_input->link);

	text_input->enable.notify = handle_text_input_enable;
	wl_signal_add(&text_input->input->events.enable, &text_input->enable);

	text_input->disable.notify = handle_text_input_disable;
	wl_signal_add(&text_input->input->events.disable, &text_input->disable);

	text_input->commit.notify = handle_text_input_commit;
	wl_signal_add(&text_input->input->events.commit, &text_input->commit);

	text_input->destroy.notify = handle_text_input_destroy;
	wl_signal_add(&text_input->input->events.destroy, &text_input->destroy);

	update_text_inputs_focused_surface(relay);
}

/*
 * Usually this function is not called because the client destroys the surface
 * role (like xdg_toplevel) first and input_method_relay_set_focus() is called
 * before wl_surface is destroyed.
 */
static void
handle_focused_surface_destroy(struct wl_listener *listener, void *data)
{
	struct input_method_relay *relay =
		wl_container_of(listener, relay, focused_surface_destroy);
	assert(relay->focused_surface == data);

	input_method_relay_set_focus(relay, NULL);
}

struct input_method_relay *
input_method_relay_create(struct seat *seat)
{
	struct input_method_relay *relay = znew(*relay);
	relay->seat = seat;
	wl_list_init(&relay->text_inputs);
	wl_list_init(&relay->popups);
	relay->popup_tree = wlr_scene_tree_create(&seat->server->scene->tree);

	relay->new_text_input.notify = handle_new_text_input;
	wl_signal_add(&seat->server->text_input_manager->events.text_input,
		&relay->new_text_input);

	relay->new_input_method.notify = handle_new_input_method;
	wl_signal_add(&seat->server->input_method_manager->events.input_method,
		&relay->new_input_method);

	relay->focused_surface_destroy.notify = handle_focused_surface_destroy;

	return relay;
}

void
input_method_relay_finish(struct input_method_relay *relay)
{
	wl_list_remove(&relay->new_text_input.link);
	wl_list_remove(&relay->new_input_method.link);
	free(relay);
}

void
input_method_relay_set_focus(struct input_method_relay *relay,
		struct wlr_surface *surface)
{
	if (relay->focused_surface == surface) {
		wlr_log(WLR_INFO, "The surface is already focused");
		return;
	}

	if (relay->focused_surface) {
		wl_list_remove(&relay->focused_surface_destroy.link);
	}
	relay->focused_surface = surface;
	if (surface) {
		wl_signal_add(&surface->events.destroy,
			&relay->focused_surface_destroy);
	}

	update_text_inputs_focused_surface(relay);
	update_active_text_input(relay);
}
