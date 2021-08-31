#define _POSIX_C_SOURCE 200809L
#include "config/mousebind.h"
#include "config/rcxml.h"
#include <wlr/util/log.h>
#include <strings.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

static enum ssd_part_type
context_from_str(const char* str)
{
	if(str == NULL) {
		return LAB_SSD_NONE;
	}
	else if(strcasecmp(str, "Titlebar") == 0) {
		return LAB_SSD_PART_TITLEBAR;
	} else {
		return LAB_SSD_NONE;
	}
}

static uint32_t
mouse_button_from_str(const char* str)
{
	if(str == NULL) {
		return UINT32_MAX;
	}
	else if(strcasecmp(str, "Left") == 0) {
		return BTN_LEFT;
	} else if(strcasecmp(str, "Right") == 0) {
		return BTN_RIGHT;
	} else if(strcasecmp(str, "Middle") == 0) {
		return BTN_MIDDLE;
	} else {
		return UINT32_MAX;
	}
}

static enum action_mouse_did
action_mouse_did_from_str(const char* str)
{
	if(str == NULL) {
		return MOUSE_ACTION_NONE;
	}
	else if(strcasecmp(str, "doubleclick") == 0) {
		return MOUSE_ACTION_DOUBLECLICK;
	} else {
		return MOUSE_ACTION_NONE;
	}
}

struct mousebind*
mousebind_create(const char* context_str, const char* mouse_button_str,
		const char* action_mouse_did_str, const char* action,
		const char* command)
{
	struct mousebind* m = calloc(1, sizeof(struct mousebind));

	enum ssd_part_type context = context_from_str(context_str);
	uint32_t button = mouse_button_from_str(mouse_button_str);
	enum action_mouse_did action_mouse_did = action_mouse_did_from_str(action_mouse_did_str);

	if(context == LAB_SSD_NONE) {
		wlr_log(WLR_ERROR, "unknown mouse context (%s)", context_str);
		goto CREATE_ERROR;
	}
	if(button == UINT32_MAX) {
		wlr_log(WLR_ERROR, "unknown button (%s)", mouse_button_str);
		goto CREATE_ERROR;
	}
	if(action_mouse_did == MOUSE_ACTION_NONE) {
		wlr_log(WLR_ERROR, "unknown mouse action (%s)", action_mouse_did_str);
		goto CREATE_ERROR;
	}
	if(action == NULL) {
		wlr_log(WLR_ERROR, "action is NULL\n");
		goto CREATE_ERROR;
	}

	m->context = context;
	m->button = button;
	m->mouse_action = action_mouse_did;
	m->action = strdup(action); /* TODO: replace with strndup? */
	if(command && !strcasecmp(action, "Execute")) {
		m->command = strdup(command);
	}

	return m;

CREATE_ERROR:
	free(m);
	return NULL;
}
