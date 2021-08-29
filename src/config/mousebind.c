#define _POSIX_C_SOURCE 200809L
#include "config/mousebind.h"
#include "config/rcxml.h"
#include <wlr/util/log.h>
#include <strings.h>
#include <unistd.h>

static enum mouse_context
context_from_str(const char* str)
{
	if(str == NULL) {
		return MOUSE_CONTEXT_NONE;
	}
	else if(strcasecmp(str, "Titlebar") == 0) {
		return MOUSE_CONTEXT_TITLEBAR;
	} else {
		return MOUSE_CONTEXT_NONE;
	}
}

static enum mouse_button
mouse_button_from_str(const char* str)
{
	if(str == NULL) {
		return MOUSE_BUTTON_NONE;
	}
	else if(strcasecmp(str, "Left") == 0) {
		return MOUSE_BUTTON_LEFT;
	} else if(strcasecmp(str, "Right") == 0) {
		return MOUSE_BUTTON_RIGHT;
	} else if(strcasecmp(str, "Middle") == 0) {
		return MOUSE_BUTTON_MIDDLE;
	} else {
		return MOUSE_BUTTON_NONE;
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

	enum mouse_context context = context_from_str(context_str);
	enum mouse_button button = mouse_button_from_str(mouse_button_str);
	enum action_mouse_did action_mouse_did = action_mouse_did_from_str(action_mouse_did_str);

	if(context == MOUSE_CONTEXT_NONE) {
		wlr_log(WLR_ERROR, "unknown mouse context (%s)", context_str);
		goto CREATE_ERROR;
	}
	if(button == MOUSE_BUTTON_NONE) {
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
	m->command = strdup(command);

	return m;

CREATE_ERROR:
	free(m);
	return NULL;
}
