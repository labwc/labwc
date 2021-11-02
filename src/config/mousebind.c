// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <linux/input-event-codes.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "config/mousebind.h"
#include "config/rcxml.h"

uint32_t
mousebind_button_from_str(const char *str)
{
	assert(str);
	if (!strcasecmp(str, "Left")) {
		return BTN_LEFT;
	} else if (!strcasecmp(str, "Right")) {
		return BTN_RIGHT;
	} else if (!strcasecmp(str, "Middle")) {
		return BTN_MIDDLE;
	}
	wlr_log(WLR_ERROR, "unknown button (%s)", str);
	return UINT32_MAX;
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
	}
	wlr_log(WLR_ERROR, "unknown mouse action (%s)", str);
	return MOUSE_ACTION_NONE;
}

static enum ssd_part_type
context_from_str(const char *str)
{
	if (!strcasecmp(str, "Titlebar")) {
		return LAB_SSD_PART_TITLEBAR;
	} else if (!strcasecmp(str, "Close")) {
		return LAB_SSD_BUTTON_CLOSE;
	} else if (!strcasecmp(str, "Maximize")) {
		return LAB_SSD_BUTTON_MAXIMIZE;
	} else if (!strcasecmp(str, "Iconify")) {
		return LAB_SSD_BUTTON_ICONIFY;
	}
	wlr_log(WLR_ERROR, "unknown mouse context (%s)", str);
	return LAB_SSD_NONE;
}

struct mousebind *
mousebind_create(const char *context)
{
	if (!context) {
		wlr_log(WLR_ERROR, "mousebind context not specified");
		return NULL;
	}
	struct mousebind *m = calloc(1, sizeof(struct mousebind));
	m->context = context_from_str(context);
	wl_list_insert(&rc.mousebinds, &m->link);
	return m;
}
