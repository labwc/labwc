// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config/mousebind.h"
#include <assert.h>
#include <linux/input-event-codes.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "config/keybind.h"
#include "config/rcxml.h"

uint32_t
mousebind_button_from_str(const char *str, uint32_t *modifiers)
{
	assert(str);

	if (modifiers) {
		*modifiers = 0;
		while (strlen(str) >= 2 && str[1] == '-') {
			char modname[2] = {str[0], 0};
			uint32_t parsed_modifier = parse_modifier(modname);
			if (!parsed_modifier) {
				goto invalid;
			}
			*modifiers |= parsed_modifier;
			str += 2;
		}
	}

	if (!strcasecmp(str, "Left")) {
		return BTN_LEFT;
	} else if (!strcasecmp(str, "Right")) {
		return BTN_RIGHT;
	} else if (!strcasecmp(str, "Middle")) {
		return BTN_MIDDLE;
	} else if (!strcasecmp(str, "Side")) {
		return BTN_SIDE;
	} else if (!strcasecmp(str, "Extra")) {
		return BTN_EXTRA;
	} else if (!strcasecmp(str, "Forward")) {
		return BTN_FORWARD;
	} else if (!strcasecmp(str, "Back")) {
		return BTN_BACK;
	} else if (!strcasecmp(str, "Task")) {
		return BTN_TASK;
	}
invalid:
	wlr_log(WLR_ERROR, "unknown button (%s)", str);
	return UINT32_MAX;
}

enum direction
mousebind_direction_from_str(const char *str, uint32_t *modifiers)
{
	assert(str);

	if (modifiers) {
		*modifiers = 0;
		while (strlen(str) >= 2 && str[1] == '-') {
			char modname[2] = {str[0], 0};
			uint32_t parsed_modifier = parse_modifier(modname);
			if (!parsed_modifier) {
				goto invalid;
			}
			*modifiers |= parsed_modifier;
			str += 2;
		}
	}

	if (!strcasecmp(str, "Left")) {
		return LAB_DIRECTION_LEFT;
	} else if (!strcasecmp(str, "Right")) {
		return LAB_DIRECTION_RIGHT;
	} else if (!strcasecmp(str, "Up")) {
		return LAB_DIRECTION_UP;
	} else if (!strcasecmp(str, "Down")) {
		return LAB_DIRECTION_DOWN;
	}
invalid:
	wlr_log(WLR_ERROR, "unknown direction (%s)", str);
	return LAB_DIRECTION_INVALID;
}

enum mouse_event
mousebind_event_from_str(const char *str)
{
	assert(str);
	if (!strcasecmp(str, "doubleclick")) {
		return MOUSE_ACTION_DOUBLECLICK;
	} else if (!strcasecmp(str, "click")) {
		return MOUSE_ACTION_CLICK;
	} else if (!strcasecmp(str, "press")) {
		return MOUSE_ACTION_PRESS;
	} else if (!strcasecmp(str, "release")) {
		return MOUSE_ACTION_RELEASE;
	} else if (!strcasecmp(str, "drag")) {
		return MOUSE_ACTION_DRAG;
	} else if (!strcasecmp(str, "scroll")) {
		return MOUSE_ACTION_SCROLL;
	} else if (!strcasecmp(str, "swipe")) {
		return MOUSE_ACTION_SWIPE;
	}
	wlr_log(WLR_ERROR, "unknown mouse action (%s)", str);
	return MOUSE_ACTION_NONE;
}

bool
mousebind_the_same(struct mousebind *a, struct mousebind *b)
{
	assert(a && b);
	return a->context == b->context
		&& a->button == b->button
		&& a->direction == b->direction
		&& a->mouse_event == b->mouse_event
		&& a->modifiers == b->modifiers
		&& a->fingers == b->fingers;
}

struct mousebind *
mousebind_create(const char *context)
{
	if (!context) {
		wlr_log(WLR_ERROR, "mousebind context not specified");
		return NULL;
	}
	struct mousebind *m = znew(*m);
	m->context = node_type_parse(context);
	if (m->context != LAB_NODE_NONE) {
		wl_list_append(&rc.mousebinds, &m->link);
	}
	wl_list_init(&m->actions);
	return m;
}
