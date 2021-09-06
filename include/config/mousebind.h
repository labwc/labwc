#ifndef __LABWC_MOUSEBIND_H
#define __LABWC_MOUSEBIND_H

#include "ssd.h"
#include <wayland-util.h>

enum mouse_event {
	MOUSE_ACTION_NONE = 0,
	MOUSE_ACTION_DOUBLECLICK,
};

struct mousebind {
	enum ssd_part_type context;

	/* ex: BTN_LEFT, BTN_RIGHT from linux/input_event_codes.h */
	uint32_t button;

	/* ex: doubleclick, press, drag */
	enum mouse_event mouse_event;
	const char *action;
	const char *command;

	struct wl_list link; /* rcxml::mousebinds */
};

enum mouse_event mousebind_event_from_str(const char *str);
uint32_t mousebind_button_from_str(const char *str);
struct mousebind *mousebind_create(const char *context);

#endif /* __LABWC_MOUSEBIND_H */
