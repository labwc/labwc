#ifndef __LABWC_MOUSEBIND_H
#define __LABWC_MOUSEBIND_H

#include <wayland-util.h>

enum mouse_context {
	MOUSE_CONTEXT_TITLEBAR,
	MOUSE_CONTEXT_NONE
};

enum mouse_button {
	/*
	 * These values match the values returned by button event->button and were
	 * obtained experimentally
	 */
	MOUSE_BUTTON_LEFT = 272,
	MOUSE_BUTTON_RIGHT = 273,
	MOUSE_BUTTON_MIDDLE = 274,
	MOUSE_BUTTON_NONE = -1
};

enum action_mouse_did {
	MOUSE_ACTION_DOUBLECLICK,
	MOUSE_ACTION_NONE
};

struct mousebind {
	enum mouse_context context; /* ex: titlebar */
	enum mouse_button button; /* ex: left, right, middle */
	enum action_mouse_did mouse_action; /* ex: doubleclick, press, drag, etc */
	const char* action; /* what to do because mouse did previous action */
	const char* command;
	struct wl_list link;
};

struct mousebind*
mousebind_create(const char* context_str, const char* mouse_button_str,
		const char* action_mouse_did_str, const char* action,
		const char* command);

#endif /* __LABWC_MOUSEBIND_H */
