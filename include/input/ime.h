/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef LABWC_IME_H
#define LABWC_IME_H

#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_input_method_v2.h>
#include "labwc.h"

struct keyboard;

/*
 * The relay structure manages the relationship between text-inputs and
 * input-method on a given seat. Multiple text-inputs may be bound to a relay,
 * but at most one will be "active" (communicating with input-method) at a time.
 * At most one input-method may be bound to the seat. When an input-method and
 * an active text-input is present, the relay passes messages between them.
 */
struct input_method_relay {
	struct seat *seat;
	struct wl_list text_inputs; /* struct text_input.link */
	struct wlr_input_method_v2 *input_method;
	struct wlr_surface *focused_surface;
	/*
	 * Text-input which is enabled by the client and communicating with
	 * input-method.
	 * This must be NULL if input-method is not present.
	 * Its client must be the same as that of focused_surface.
	 */
	struct text_input *active_text_input;

	struct wlr_input_popup_surface_v2 *popup_surface;
	struct wlr_scene_tree *popup_tree;

	struct wl_listener new_text_input;
	struct wl_listener new_input_method;

	struct wl_listener input_method_commit;
	struct wl_listener input_method_grab_keyboard;
	struct wl_listener input_method_destroy;
	struct wl_listener input_method_new_popup_surface;

	struct wl_listener popup_surface_destroy;
	struct wl_listener popup_surface_commit;

	struct wl_listener keyboard_grab_destroy;
	struct wl_listener focused_surface_destroy;
};

struct text_input {
	struct input_method_relay *relay;
	struct wlr_text_input_v3 *input;
	struct wl_list link;

	struct wl_listener enable;
	struct wl_listener commit;
	struct wl_listener disable;
	struct wl_listener destroy;
};

/*
 * Forward key event to keyboard grab of the seat from the keyboard
 * if the keyboard grab exists.
 * Returns true if the key event was forwarded.
 */
bool input_method_keyboard_grab_forward_key(struct keyboard *keyboard,
	struct wlr_keyboard_key_event *event);

/*
 * Forward modifier state to keyboard grab of the seat from the keyboard
 * if the keyboard grab exists.
 * Returns true if the modifier state was forwarded.
 */
bool input_method_keyboard_grab_forward_modifiers(struct keyboard *keyboard);

struct input_method_relay *input_method_relay_create(struct seat *seat);

void input_method_relay_finish(struct input_method_relay *relay);

/* Updates currently focused surface. Surface must belong to the same seat. */
void input_method_relay_set_focus(struct input_method_relay *relay,
	struct wlr_surface *surface);

#endif
