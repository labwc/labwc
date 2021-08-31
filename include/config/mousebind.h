#ifndef __LABWC_MOUSEBIND_H
#define __LABWC_MOUSEBIND_H

#include "ssd.h"
#include <wayland-util.h>

enum action_mouse_did
{
	MOUSE_ACTION_DOUBLECLICK,
	MOUSE_ACTION_NONE
};

struct mousebind {
	enum ssd_part_type context; /* ex: titlebar */

	/* ex: BTN_LEFT, BTN_RIGHT from linux/input_event_codes.h */
	uint32_t button;

	/* ex: doubleclick, press, drag, etc */
	enum action_mouse_did mouse_action;

	/* what to do because mouse did previous action */
	const char *action;

	const char *command;
	struct wl_list link;
};

struct mousebind *
mousebind_create(const char *context_str, const char *mouse_button_str,
    const char *action_mouse_did_str, const char *action, const char *command);

#endif /* __LABWC_MOUSEBIND_H */
