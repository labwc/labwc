/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_MOUSEBIND_H
#define __LABWC_MOUSEBIND_H

#include <wayland-util.h>
#include "ssd.h"
#include "config/keybind.h"

enum mouse_event {
	MOUSE_ACTION_NONE = 0,
	MOUSE_ACTION_DOUBLECLICK,
	MOUSE_ACTION_CLICK,
	MOUSE_ACTION_PRESS,
	MOUSE_ACTION_RELEASE,
	MOUSE_ACTION_DRAG,
	MOUSE_ACTION_SCROLL,
};

enum direction {
	LAB_DIRECTION_INVALID = 0,
	LAB_DIRECTION_LEFT,
	LAB_DIRECTION_RIGHT,
	LAB_DIRECTION_UP,
	LAB_DIRECTION_DOWN,
};

struct mousebind {
	enum ssd_part_type context;

	/* ex: BTN_LEFT, BTN_RIGHT from linux/input_event_codes.h */
	uint32_t button;

	/* scroll direction; considered instead of button for scroll events */
	enum direction direction;

	/* ex: WLR_MODIFIER_SHIFT | WLR_MODIFIER_LOGO */
	uint32_t modifiers;

	/* ex: doubleclick, press, drag */
	enum mouse_event mouse_event;
	struct wl_list actions;  /* struct action.link */

	struct wl_list link;     /* struct rcxml.mousebinds */
	bool pressed_in_context; /* used in click events */
};

enum mouse_event mousebind_event_from_str(const char *str);
uint32_t mousebind_button_from_str(const char *str, uint32_t *modifiers);
enum direction mousebind_direction_from_str(const char *str, uint32_t *modifiers);
struct mousebind *mousebind_create(const char *context);
bool mousebind_the_same(struct mousebind *a, struct mousebind *b);

#endif /* __LABWC_MOUSEBIND_H */
