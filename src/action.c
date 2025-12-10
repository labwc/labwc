// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "action.h"
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "action-prompt-codes.h"
#include "common/buf.h"
#include "common/macros.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "cycle.h"
#include "debug.h"
#include "input/keyboard.h"
#include "labwc.h"
#include "magnifier.h"
#include "menu/menu.h"
#include "output.h"
#include "output-virtual.h"
#include "regions.h"
#include "ssd.h"
#include "theme.h"
#include "translate.h"
#include "view.h"
#include "workspaces.h"

enum action_arg_type {
	LAB_ACTION_ARG_STR = 0,
	LAB_ACTION_ARG_BOOL,
	LAB_ACTION_ARG_INT,
	LAB_ACTION_ARG_QUERY_LIST,
	LAB_ACTION_ARG_ACTION_LIST,
};

struct action_arg {
	struct wl_list link;        /* struct action.args */

	char *key;                  /* May be NULL if there is just one arg */
	enum action_arg_type type;
};

struct action_arg_str {
	struct action_arg base;
	char *value;
};

struct action_arg_bool {
	struct action_arg base;
	bool value;
};

struct action_arg_int {
	struct action_arg base;
	int value;
};

struct action_arg_list {
	struct action_arg base;
	struct wl_list value;
};

enum action_type {
	ACTION_TYPE_INVALID = 0,
	ACTION_TYPE_NONE,
	ACTION_TYPE_CLOSE,
	ACTION_TYPE_KILL,
	ACTION_TYPE_DEBUG,
	ACTION_TYPE_EXECUTE,
	ACTION_TYPE_EXIT,
	ACTION_TYPE_MOVE_TO_EDGE,
	ACTION_TYPE_TOGGLE_SNAP_TO_EDGE,
	ACTION_TYPE_SNAP_TO_EDGE,
	ACTION_TYPE_GROW_TO_EDGE,
	ACTION_TYPE_SHRINK_TO_EDGE,
	ACTION_TYPE_NEXT_WINDOW,
	ACTION_TYPE_PREVIOUS_WINDOW,
	ACTION_TYPE_RECONFIGURE,
	ACTION_TYPE_SHOW_MENU,
	ACTION_TYPE_TOGGLE_MAXIMIZE,
	ACTION_TYPE_MAXIMIZE,
	ACTION_TYPE_UNMAXIMIZE,
	ACTION_TYPE_TOGGLE_FULLSCREEN,
	ACTION_TYPE_SET_DECORATIONS,
	ACTION_TYPE_TOGGLE_DECORATIONS,
	ACTION_TYPE_TOGGLE_ALWAYS_ON_TOP,
	ACTION_TYPE_TOGGLE_ALWAYS_ON_BOTTOM,
	ACTION_TYPE_TOGGLE_OMNIPRESENT,
	ACTION_TYPE_FOCUS,
	ACTION_TYPE_UNFOCUS,
	ACTION_TYPE_ICONIFY,
	ACTION_TYPE_MOVE,
	ACTION_TYPE_RAISE,
	ACTION_TYPE_LOWER,
	ACTION_TYPE_RESIZE,
	ACTION_TYPE_RESIZE_RELATIVE,
	ACTION_TYPE_MOVETO,
	ACTION_TYPE_RESIZETO,
	ACTION_TYPE_MOVETO_CURSOR,
	ACTION_TYPE_MOVE_RELATIVE,
	ACTION_TYPE_SEND_TO_DESKTOP,
	ACTION_TYPE_GO_TO_DESKTOP,
	ACTION_TYPE_TOGGLE_SNAP_TO_REGION,
	ACTION_TYPE_SNAP_TO_REGION,
	ACTION_TYPE_UNSNAP,
	ACTION_TYPE_TOGGLE_KEYBINDS,
	ACTION_TYPE_FOCUS_OUTPUT,
	ACTION_TYPE_MOVE_TO_OUTPUT,
	ACTION_TYPE_FIT_TO_OUTPUT,
	ACTION_TYPE_IF,
	ACTION_TYPE_FOR_EACH,
	ACTION_TYPE_VIRTUAL_OUTPUT_ADD,
	ACTION_TYPE_VIRTUAL_OUTPUT_REMOVE,
	ACTION_TYPE_AUTO_PLACE,
	ACTION_TYPE_TOGGLE_TEARING,
	ACTION_TYPE_SHADE,
	ACTION_TYPE_UNSHADE,
	ACTION_TYPE_TOGGLE_SHADE,
	ACTION_TYPE_ENABLE_SCROLL_WHEEL_EMULATION,
	ACTION_TYPE_DISABLE_SCROLL_WHEEL_EMULATION,
	ACTION_TYPE_TOGGLE_SCROLL_WHEEL_EMULATION,
	ACTION_TYPE_ENABLE_TABLET_MOUSE_EMULATION,
	ACTION_TYPE_DISABLE_TABLET_MOUSE_EMULATION,
	ACTION_TYPE_TOGGLE_TABLET_MOUSE_EMULATION,
	ACTION_TYPE_TOGGLE_MAGNIFY,
	ACTION_TYPE_ZOOM_IN,
	ACTION_TYPE_ZOOM_OUT,
	ACTION_TYPE_WARP_CURSOR,
	ACTION_TYPE_HIDE_CURSOR,
};

const char *action_names[] = {
	"INVALID",
	"None",
	"Close",
	"Kill",
	"Debug",
	"Execute",
	"Exit",
	"MoveToEdge",
	"ToggleSnapToEdge",
	"SnapToEdge",
	"GrowToEdge",
	"ShrinkToEdge",
	"NextWindow",
	"PreviousWindow",
	"Reconfigure",
	"ShowMenu",
	"ToggleMaximize",
	"Maximize",
	"UnMaximize",
	"ToggleFullscreen",
	"SetDecorations",
	"ToggleDecorations",
	"ToggleAlwaysOnTop",
	"ToggleAlwaysOnBottom",
	"ToggleOmnipresent",
	"Focus",
	"Unfocus",
	"Iconify",
	"Move",
	"Raise",
	"Lower",
	"Resize",
	"ResizeRelative",
	"MoveTo",
	"ResizeTo",
	"MoveToCursor",
	"MoveRelative",
	"SendToDesktop",
	"GoToDesktop",
	"ToggleSnapToRegion",
	"SnapToRegion",
	"UnSnap",
	"ToggleKeybinds",
	"FocusOutput",
	"MoveToOutput",
	"FitToOutput",
	"If",
	"ForEach",
	"VirtualOutputAdd",
	"VirtualOutputRemove",
	"AutoPlace",
	"ToggleTearing",
	"Shade",
	"Unshade",
	"ToggleShade",
	"EnableScrollWheelEmulation",
	"DisableScrollWheelEmulation",
	"ToggleScrollWheelEmulation",
	"EnableTabletMouseEmulation",
	"DisableTabletMouseEmulation",
	"ToggleTabletMouseEmulation",
	"ToggleMagnify",
	"ZoomIn",
	"ZoomOut",
	"WarpCursor",
	"HideCursor",
	NULL
};

void
action_arg_add_str(struct action *action, const char *key, const char *value)
{
	assert(action);
	assert(key);
	assert(value && "Tried to add NULL action string argument");
	struct action_arg_str *arg = znew(*arg);
	arg->base.type = LAB_ACTION_ARG_STR;
	arg->base.key = xstrdup(key);
	arg->value = xstrdup(value);
	wl_list_append(&action->args, &arg->base.link);
}

static void
action_arg_add_bool(struct action *action, const char *key, bool value)
{
	assert(action);
	assert(key);
	struct action_arg_bool *arg = znew(*arg);
	arg->base.type = LAB_ACTION_ARG_BOOL;
	arg->base.key = xstrdup(key);
	arg->value = value;
	wl_list_append(&action->args, &arg->base.link);
}

static void
action_arg_add_int(struct action *action, const char *key, int value)
{
	assert(action);
	assert(key);
	struct action_arg_int *arg = znew(*arg);
	arg->base.type = LAB_ACTION_ARG_INT;
	arg->base.key = xstrdup(key);
	arg->value = value;
	wl_list_append(&action->args, &arg->base.link);
}

static void
action_arg_add_list(struct action *action, const char *key, enum action_arg_type type)
{
	assert(action);
	assert(key);
	struct action_arg_list *arg = znew(*arg);
	arg->base.type = type;
	arg->base.key = xstrdup(key);
	wl_list_init(&arg->value);
	wl_list_append(&action->args, &arg->base.link);
}

void
action_arg_add_querylist(struct action *action, const char *key)
{
	action_arg_add_list(action, key, LAB_ACTION_ARG_QUERY_LIST);
}

void
action_arg_add_actionlist(struct action *action, const char *key)
{
	action_arg_add_list(action, key, LAB_ACTION_ARG_ACTION_LIST);
}

static void *
action_get_arg(struct action *action, const char *key, enum action_arg_type type)
{
	assert(action);
	assert(key);
	struct action_arg *arg;
	wl_list_for_each(arg, &action->args, link) {
		if (!strcasecmp(key, arg->key) && arg->type == type) {
			return arg;
		}
	}
	return NULL;
}

const char *
action_get_str(struct action *action, const char *key, const char *default_value)
{
	struct action_arg_str *arg = action_get_arg(action, key, LAB_ACTION_ARG_STR);
	return arg ? arg->value : default_value;
}

static bool
action_get_bool(struct action *action, const char *key, bool default_value)
{
	struct action_arg_bool *arg = action_get_arg(action, key, LAB_ACTION_ARG_BOOL);
	return arg ? arg->value : default_value;
}

static int
action_get_int(struct action *action, const char *key, int default_value)
{
	struct action_arg_int *arg = action_get_arg(action, key, LAB_ACTION_ARG_INT);
	return arg ? arg->value : default_value;
}

struct wl_list *
action_get_querylist(struct action *action, const char *key)
{
	struct action_arg_list *arg = action_get_arg(action, key, LAB_ACTION_ARG_QUERY_LIST);
	return arg ? &arg->value : NULL;
}

struct wl_list *
action_get_actionlist(struct action *action, const char *key)
{
	struct action_arg_list *arg = action_get_arg(action, key, LAB_ACTION_ARG_ACTION_LIST);
	return arg ? &arg->value : NULL;
}

void
action_arg_from_xml_node(struct action *action, const char *nodename, const char *content)
{
	assert(action);

	char *argument = xstrdup(nodename);
	string_truncate_at_pattern(argument, ".action");

	switch (action->type) {
	case ACTION_TYPE_EXECUTE:
		/*
		 * <action name="Execute"> with an <execute> child is
		 * deprecated, but we support it anyway for backward
		 * compatibility with old openbox-menu generators
		 */
		if (!strcmp(argument, "command") || !strcmp(argument, "execute")) {
			action_arg_add_str(action, "command", content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_MOVE_TO_EDGE:
	case ACTION_TYPE_TOGGLE_SNAP_TO_EDGE:
	case ACTION_TYPE_SNAP_TO_EDGE:
	case ACTION_TYPE_GROW_TO_EDGE:
	case ACTION_TYPE_SHRINK_TO_EDGE:
		if (!strcmp(argument, "direction")) {
			bool tiled = (action->type == ACTION_TYPE_TOGGLE_SNAP_TO_EDGE
					|| action->type == ACTION_TYPE_SNAP_TO_EDGE);
			enum lab_edge edge = lab_edge_parse(content, tiled, /*any*/ false);
			if (edge == LAB_EDGE_NONE) {
				wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s' (%s)",
					action_names[action->type], argument, content);
			} else {
				action_arg_add_int(action, argument, edge);
			}
			goto cleanup;
		}
		if (action->type == ACTION_TYPE_MOVE_TO_EDGE
				&& !strcasecmp(argument, "snapWindows")) {
			action_arg_add_bool(action, argument, parse_bool(content, true));
			goto cleanup;
		}
		if ((action->type == ACTION_TYPE_SNAP_TO_EDGE
					|| action->type == ACTION_TYPE_TOGGLE_SNAP_TO_EDGE)
				&& !strcasecmp(argument, "combine")) {
			action_arg_add_bool(action, argument, parse_bool(content, false));
			goto cleanup;
		}
		break;
	case ACTION_TYPE_SHOW_MENU:
		if (!strcmp(argument, "menu")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		if (!strcasecmp(argument, "atCursor")) {
			action_arg_add_bool(action, argument, parse_bool(content, true));
			goto cleanup;
		}
		if (!strcasecmp(argument, "x.position")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		if (!strcasecmp(argument, "y.position")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_TOGGLE_MAXIMIZE:
	case ACTION_TYPE_MAXIMIZE:
	case ACTION_TYPE_UNMAXIMIZE:
		if (!strcmp(argument, "direction")) {
			enum view_axis axis = view_axis_parse(content);
			if (axis == VIEW_AXIS_NONE || axis == VIEW_AXIS_INVALID) {
				wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s' (%s)",
					action_names[action->type], argument, content);
			} else {
				action_arg_add_int(action, argument, axis);
			}
			goto cleanup;
		}
		break;
	case ACTION_TYPE_SET_DECORATIONS:
		if (!strcmp(argument, "decorations")) {
			enum lab_ssd_mode mode = ssd_mode_parse(content);
			if (mode != LAB_SSD_MODE_INVALID) {
				action_arg_add_int(action, argument, mode);
			} else {
				wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s' (%s)",
					action_names[action->type], argument, content);
			}
			goto cleanup;
		}
		if (!strcasecmp(argument, "forceSSD")) {
			action_arg_add_bool(action, argument, parse_bool(content, false));
			goto cleanup;
		}
		break;
	case ACTION_TYPE_RESIZE:
		if (!strcmp(argument, "direction")) {
			enum lab_edge edge = lab_edge_parse(content,
				/*tiled*/ true, /*any*/ false);
			if (edge == LAB_EDGE_NONE || edge == LAB_EDGE_CENTER) {
				wlr_log(WLR_ERROR,
					"Invalid argument for action %s: '%s' (%s)",
					action_names[action->type], argument, content);
			} else {
				action_arg_add_int(action, argument, edge);
			}
			goto cleanup;
		}
		break;
	case ACTION_TYPE_RESIZE_RELATIVE:
		if (!strcmp(argument, "left") || !strcmp(argument, "right") ||
				!strcmp(argument, "top") || !strcmp(argument, "bottom")) {
			action_arg_add_int(action, argument, atoi(content));
			goto cleanup;
		}
		break;
	case ACTION_TYPE_MOVETO:
	case ACTION_TYPE_MOVE_RELATIVE:
		if (!strcmp(argument, "x") || !strcmp(argument, "y")) {
			action_arg_add_int(action, argument, atoi(content));
			goto cleanup;
		}
		break;
	case ACTION_TYPE_RESIZETO:
		if (!strcmp(argument, "width") || !strcmp(argument, "height")) {
			action_arg_add_int(action, argument, atoi(content));
			goto cleanup;
		}
		break;
	case ACTION_TYPE_SEND_TO_DESKTOP:
		if (!strcmp(argument, "follow")) {
			action_arg_add_bool(action, argument, parse_bool(content, true));
			goto cleanup;
		}
		/* Falls through to GoToDesktop */
	case ACTION_TYPE_GO_TO_DESKTOP:
		if (!strcmp(argument, "to")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		if (!strcmp(argument, "wrap")) {
			action_arg_add_bool(action, argument, parse_bool(content, true));
			goto cleanup;
		}
		if (!strcmp(argument, "toggle")) {
			action_arg_add_bool(
				action, argument, parse_bool(content, false));
			goto cleanup;
		}
		break;
	case ACTION_TYPE_TOGGLE_SNAP_TO_REGION:
	case ACTION_TYPE_SNAP_TO_REGION:
		if (!strcmp(argument, "region")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_FOCUS_OUTPUT:
	case ACTION_TYPE_MOVE_TO_OUTPUT:
		if (!strcmp(argument, "output")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		if (!strcmp(argument, "direction")) {
			enum lab_edge edge = lab_edge_parse(content,
				/*tiled*/ false, /*any*/ false);
			if (edge == LAB_EDGE_NONE) {
				wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s' (%s)",
					action_names[action->type], argument, content);
			} else {
				action_arg_add_int(action, argument, edge);
			}
			goto cleanup;
		}
		if (!strcmp(argument, "wrap")) {
			action_arg_add_bool(action, argument, parse_bool(content, false));
			goto cleanup;
		}
		break;
	case ACTION_TYPE_VIRTUAL_OUTPUT_ADD:
	case ACTION_TYPE_VIRTUAL_OUTPUT_REMOVE:
		if (!strcmp(argument, "output_name")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_AUTO_PLACE:
		if (!strcmp(argument, "policy")) {
			enum lab_placement_policy policy =
				view_placement_parse(content);
			if (policy == LAB_PLACE_INVALID) {
				wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s' (%s)",
						action_names[action->type], argument, content);
			} else {
				action_arg_add_int(action, argument, policy);
			}
			goto cleanup;
		}
		break;
	case ACTION_TYPE_WARP_CURSOR:
		if (!strcmp(argument, "to") || !strcmp(argument, "x") || !strcmp(argument, "y")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_IF:
		if (!strcmp(argument, "message.prompt")) {
			action_arg_add_str(action, "message.prompt", content);
		}
		goto cleanup;
	}

	wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s'",
		action_names[action->type], argument);

cleanup:
	free(argument);
}

static enum action_type
action_type_from_str(const char *action_name)
{
	for (size_t i = 1; action_names[i]; i++) {
		if (!strcasecmp(action_name, action_names[i])) {
			return i;
		}
	}
	wlr_log(WLR_ERROR, "Invalid action: %s", action_name);
	return ACTION_TYPE_INVALID;
}

struct action *
action_create(const char *action_name)
{
	if (!action_name) {
		wlr_log(WLR_ERROR, "action name not specified");
		return NULL;
	}

	enum action_type action_type = action_type_from_str(action_name);
	if (action_type == ACTION_TYPE_NONE) {
		return NULL;
	}

	struct action *action = znew(*action);
	action->type = action_type;
	wl_list_init(&action->args);
	return action;
}

bool
actions_contain_toggle_keybinds(struct wl_list *action_list)
{
	struct action *action;
	wl_list_for_each(action, action_list, link) {
		if (action->type == ACTION_TYPE_TOGGLE_KEYBINDS) {
			return true;
		}
	}
	return false;
}

static bool
action_list_is_valid(struct wl_list *actions)
{
	assert(actions);

	struct action *action;
	wl_list_for_each(action, actions, link) {
		if (!action_is_valid(action)) {
			return false;
		}
	}
	return true;
}

static bool
action_branches_are_valid(struct action *action)
{
	static const char * const branches[] = { "then", "else", "none" };
	for (size_t i = 0; i < ARRAY_SIZE(branches); i++) {
		struct wl_list *children =
			action_get_actionlist(action, branches[i]);
		if (children && !action_list_is_valid(children)) {
			wlr_log(WLR_ERROR, "Invalid action in %s '%s' branch",
				action_names[action->type], branches[i]);
			return false;
		}
	}
	return true;
}

/* Checks for *required* arguments */
bool
action_is_valid(struct action *action)
{
	const char *arg_name = NULL;
	enum action_arg_type arg_type = LAB_ACTION_ARG_STR;

	switch (action->type) {
	case ACTION_TYPE_EXECUTE:
		arg_name = "command";
		break;
	case ACTION_TYPE_MOVE_TO_EDGE:
	case ACTION_TYPE_TOGGLE_SNAP_TO_EDGE:
	case ACTION_TYPE_SNAP_TO_EDGE:
	case ACTION_TYPE_GROW_TO_EDGE:
	case ACTION_TYPE_SHRINK_TO_EDGE:
		arg_name = "direction";
		arg_type = LAB_ACTION_ARG_INT;
		break;
	case ACTION_TYPE_SHOW_MENU:
		arg_name = "menu";
		break;
	case ACTION_TYPE_GO_TO_DESKTOP:
	case ACTION_TYPE_SEND_TO_DESKTOP:
		arg_name = "to";
		break;
	case ACTION_TYPE_TOGGLE_SNAP_TO_REGION:
	case ACTION_TYPE_SNAP_TO_REGION:
		arg_name = "region";
		break;
	case ACTION_TYPE_IF:
	case ACTION_TYPE_FOR_EACH:
		return action_branches_are_valid(action);
	default:
		/* No arguments required */
		return true;
	}

	if (action_get_arg(action, arg_name, arg_type)) {
		return true;
	}

	wlr_log(WLR_ERROR, "Missing required argument for %s: %s",
		action_names[action->type], arg_name);
	return false;
}

bool
action_is_show_menu(struct action *action)
{
	return action->type == ACTION_TYPE_SHOW_MENU;
}

void
action_free(struct action *action)
{
	/* Free args */
	struct action_arg *arg, *arg_tmp;
	wl_list_for_each_safe(arg, arg_tmp, &action->args, link) {
		wl_list_remove(&arg->link);
		zfree(arg->key);
		if (arg->type == LAB_ACTION_ARG_STR) {
			struct action_arg_str *str_arg = (struct action_arg_str *)arg;
			zfree(str_arg->value);
		} else if (arg->type == LAB_ACTION_ARG_ACTION_LIST) {
			struct action_arg_list *list_arg = (struct action_arg_list *)arg;
			action_list_free(&list_arg->value);
		} else if (arg->type == LAB_ACTION_ARG_QUERY_LIST) {
			struct action_arg_list *list_arg = (struct action_arg_list *)arg;
			struct view_query *elm, *next;
			wl_list_for_each_safe(elm, next, &list_arg->value, link) {
				view_query_free(elm);
			}
		}
		zfree(arg);
	}
	zfree(action);
}

void
action_list_free(struct wl_list *action_list)
{
	struct action *action, *action_tmp;
	wl_list_for_each_safe(action, action_tmp, action_list, link) {
		wl_list_remove(&action->link);
		action_free(action);
	}
}

static void
show_menu(struct server *server, struct view *view, struct cursor_context *ctx,
		const char *menu_name, bool at_cursor,
		const char *pos_x, const char *pos_y)
{
	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH
			&& server->input_mode != LAB_INPUT_STATE_MENU) {
		/* Prevent opening a menu while resizing / moving a view */
		return;
	}

	struct menu *menu = menu_get_by_id(server, menu_name);
	if (!menu) {
		return;
	}

	int x = server->seat.cursor->x;
	int y = server->seat.cursor->y;

	/* The client menu needs an active client */
	bool is_client_menu = !strcasecmp(menu_name, "client-menu");
	if (is_client_menu && !view) {
		return;
	}
	/* Place menu in the view corner if desired (and menu is not root-menu) */
	if (!at_cursor && view) {
		struct wlr_box extent = ssd_max_extents(view);
		x = extent.x;
		y = view->current.y;
		/* Push the client menu underneath the button */
		if (is_client_menu && node_type_contains(
				LAB_NODE_BUTTON, ctx->type)) {
			assert(ctx->node);
			int lx, ly;
			wlr_scene_node_coords(ctx->node, &lx, &ly);
			/* MAX() prevents negative x when the window is maximized */
			x = MAX(x, lx - server->theme->menu_border_width);
		}
	}

	/*
	 * determine placement by looking at x and y
	 * x/y can be number, "center" or a %percent of screen dimensions
	 */
	if (pos_x && pos_y) {
		struct output *output = output_nearest_to(server,
				server->seat.cursor->x, server->seat.cursor->y);
		struct wlr_box usable = output_usable_area_in_layout_coords(output);

		if (!strcasecmp(pos_x, "center")) {
			x = (usable.width - menu->size.width) / 2;
		} else if (strchr(pos_x, '%')) {
			x = (usable.width * atoi(pos_x)) / 100;
		} else {
			if (pos_x[0] == '-') {
				int neg_x = strtol(pos_x, NULL, 10);
				x = usable.width + neg_x;
			} else {
				x = atoi(pos_x);
			}
		}

		if (!strcasecmp(pos_y, "center")) {
			y = (usable.height / 2) - (menu->size.height / 2);
		} else if (strchr(pos_y, '%')) {
			y = (usable.height * atoi(pos_y)) / 100;
		} else {
			if (pos_y[0] == '-') {
				int neg_y = strtol(pos_y, NULL, 10);
				y = usable.height + neg_y;
			} else {
				y = atoi(pos_y);
			}
		}
		/* keep menu from being off screen */
		x = MAX(x, 0);
		x = MIN(x, usable.width - 1);
		y = MAX(y, 0);
		y = MIN(y, usable.height - 1);
		/* adjust for which monitor to appear on */
		x += usable.x;
		y += usable.y;
	}

	/* Replaced by next show_menu() or cleaned on view_destroy() */
	menu->triggered_by_view = view;
	menu_open_root(menu, x, y);
}

static struct view *
view_for_action(struct view *activator, struct server *server,
	struct action *action, struct cursor_context *ctx)
{
	/* View is explicitly specified for mousebinds */
	if (activator) {
		return activator;
	}

	/* Select view based on action type for keybinds */
	switch (action->type) {
	case ACTION_TYPE_FOCUS:
	case ACTION_TYPE_MOVE:
	case ACTION_TYPE_RESIZE: {
		*ctx = get_cursor_context(server);
		return ctx->view;
	}
	default:
		return server->active_view;
	}
}

struct action_prompt {
	/* Set when created */
	struct server *server;
	struct action *action;
	struct view *view;

	/* Set when executed */
	pid_t pid;

	struct {
		struct wl_listener destroy;
	} on_view;
	struct wl_list link;
};

static struct wl_list prompts = WL_LIST_INIT(&prompts);

static void
action_prompt_destroy(struct action_prompt *prompt)
{
	wl_list_remove(&prompt->on_view.destroy.link);
	wl_list_remove(&prompt->link);
	free(prompt);
}

static void
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct action_prompt *prompt = wl_container_of(listener, prompt, on_view.destroy);
	wl_list_remove(&prompt->on_view.destroy.link);
	wl_list_init(&prompt->on_view.destroy.link);
	prompt->view = NULL;
}

static void
print_prompt_command(struct buf *buf, const char *format,
		struct action *action, struct theme *theme)
{
	assert(format);

	for (const char *p = format; *p; p++) {
		/*
		 * If we're not on a conversion specifier (like %m) then just
		 * keep adding it to the buffer
		 */
		if (*p != '%') {
			buf_add_char(buf, *p);
			continue;
		}

		/* Process the %* conversion specifier */
		++p;

		switch (*p) {
		case 'm':
			buf_add(buf, action_get_str(action,
					"message.prompt", "Choose wisely"));
			break;
		case 'n':
			buf_add(buf, _("No"));
			break;
		case 'y':
			buf_add(buf, _("Yes"));
			break;
		case 'b':
			buf_add_hex_color(buf, theme->osd_bg_color);
			break;
		case 't':
			buf_add_hex_color(buf, theme->osd_label_text_color);
			break;
		default:
			wlr_log(WLR_ERROR,
				"invalid prompt command conversion specifier '%c'", *p);
			break;
		}
	}
}

static void
action_prompt_create(struct view *view, struct server *server, struct action *action)
{
	struct buf command = BUF_INIT;
	print_prompt_command(&command, rc.prompt_command, action, rc.theme);

	wlr_log(WLR_INFO, "prompt command: '%s'", command.data);

	int pipe_fd;
	pid_t prompt_pid = spawn_piped(command.data, &pipe_fd);
	if (prompt_pid < 0) {
		wlr_log(WLR_ERROR, "Failed to create action prompt");
		goto cleanup;
	}
	/* FIXME: closing stdout might confuse clients */
	close(pipe_fd);

	struct action_prompt *prompt = znew(*prompt);
	prompt->server = server;
	prompt->action = action;
	prompt->view = view;
	prompt->pid = prompt_pid;
	if (view) {
		prompt->on_view.destroy.notify = handle_view_destroy;
		wl_signal_add(&view->events.destroy, &prompt->on_view.destroy);
	} else {
		/* Allows removing during destroy */
		wl_list_init(&prompt->on_view.destroy.link);
	}

	wl_list_insert(&prompts, &prompt->link);

cleanup:
	buf_reset(&command);
}

void
action_prompts_destroy(void)
{
	struct action_prompt *prompt, *tmp;
	wl_list_for_each_safe(prompt, tmp, &prompts, link) {
		action_prompt_destroy(prompt);
	}
}

bool
action_check_prompt_result(pid_t pid, int exit_code)
{
	struct action_prompt *prompt, *tmp;
	wl_list_for_each_safe(prompt, tmp, &prompts, link) {
		if (prompt->pid != pid) {
			continue;
		}

		wlr_log(WLR_INFO, "Found pending prompt for exit code %d", exit_code);
		struct wl_list *actions = NULL;
		if (exit_code == LAB_EXIT_SUCCESS) {
			wlr_log(WLR_INFO, "Selected the 'then' branch");
			actions = action_get_actionlist(prompt->action, "then");
		} else if (exit_code == LAB_EXIT_CANCELLED) {
			/* no-op */
		} else {
			wlr_log(WLR_INFO, "Selected the 'else' branch");
			actions = action_get_actionlist(prompt->action, "else");
		}
		if (actions) {
			wlr_log(WLR_INFO, "Running actions");
			actions_run(prompt->view, prompt->server,
				actions, /*cursor_ctx*/ NULL);
		} else {
			wlr_log(WLR_INFO, "No actions for selected branch");
		}
		action_prompt_destroy(prompt);
		return true;
	}
	return false;
}

static bool
match_queries(struct view *view, struct action *action)
{
	assert(view);

	struct wl_list *queries = action_get_querylist(action, "query");
	if (!queries) {
		return true;
	}

	/* All queries are OR'ed */
	struct view_query *query;
	wl_list_for_each(query, queries, link) {
		if (view_matches_query(view, query)) {
			return true;
		}
	}
	return false;
}

static struct output *
get_target_output(struct output *output, struct server *server,
	struct action *action)
{
	const char *output_name = action_get_str(action, "output", NULL);
	struct output *target = NULL;

	if (output_name) {
		target = output_from_name(server, output_name);
	} else {
		enum lab_edge edge =
			action_get_int(action, "direction", LAB_EDGE_NONE);
		bool wrap = action_get_bool(action, "wrap", false);
		target = output_get_adjacent(output, edge, wrap);
	}

	if (!target) {
		wlr_log(WLR_DEBUG, "Invalid output");
	}

	return target;
}

static void
warp_cursor(struct server *server, struct view *view, const char *to, const char *x, const char *y)
{
	struct output *output = output_nearest_to_cursor(server);
	struct wlr_box target_area = {0};
	int goto_x;
	int goto_y;

	if (!strcasecmp(to, "output") && output) {
		target_area = output_usable_area_in_layout_coords(output);
	} else if (!strcasecmp(to, "window") && view) {
		target_area = view->current;
	} else {
		wlr_log(WLR_ERROR, "Invalid argument for action WarpCursor: 'to' (%s)", to);
	}

	if (!strcasecmp(x, "center")) {
		goto_x = target_area.x + target_area.width / 2;
	} else {
		int offset_x = atoi(x);
		goto_x = offset_x >= 0 ?
			target_area.x + offset_x :
			target_area.x + target_area.width + offset_x;
	}

	if (!strcasecmp(y, "center")) {
		goto_y = target_area.y + target_area.height / 2;
	} else {
		int offset_y = atoi(y);
		goto_y = offset_y >= 0 ?
			target_area.y + offset_y :
			target_area.y + target_area.height + offset_y;
	}

	wlr_cursor_warp(server->seat.cursor, NULL, goto_x, goto_y);
	cursor_update_focus(server);
}

static void
run_action(struct view *view, struct server *server, struct action *action,
	struct cursor_context *ctx)
{
	switch (action->type) {
	case ACTION_TYPE_CLOSE:
		if (view) {
			view_close(view);
		}
		break;
	case ACTION_TYPE_KILL:
		if (view) {
			/* Send SIGTERM to the process associated with the surface */
			assert(view->impl->get_pid);
			pid_t pid = view->impl->get_pid(view);
			if (pid == getpid()) {
				wlr_log(WLR_ERROR, "Preventing sending SIGTERM to labwc");
			} else if (pid > 0) {
				kill(pid, SIGTERM);
			}
		}
		break;
	case ACTION_TYPE_DEBUG:
		debug_dump_scene(server);
		break;
	case ACTION_TYPE_EXECUTE: {
		struct buf cmd = BUF_INIT;
		buf_add(&cmd, action_get_str(action, "command", NULL));
		buf_expand_tilde(&cmd);
		spawn_async_no_shell(cmd.data);
		buf_reset(&cmd);
		break;
	}
	case ACTION_TYPE_EXIT:
		wl_display_terminate(server->wl_display);
		break;
	case ACTION_TYPE_MOVE_TO_EDGE:
		if (view) {
			/* Config parsing makes sure that direction is a valid direction */
			enum lab_edge edge = action_get_int(action, "direction", 0);
			bool snap_to_windows = action_get_bool(action, "snapWindows", true);
			view_move_to_edge(view, edge, snap_to_windows);
		}
		break;
	case ACTION_TYPE_TOGGLE_SNAP_TO_EDGE:
	case ACTION_TYPE_SNAP_TO_EDGE:
		if (view) {
			/* Config parsing makes sure that direction is a valid direction */
			enum lab_edge edge = action_get_int(action, "direction", 0);
			if (action->type == ACTION_TYPE_TOGGLE_SNAP_TO_EDGE
					&& view->maximized == VIEW_AXIS_NONE
					&& !view->fullscreen
					&& view_is_tiled(view)
					&& view->tiled == edge) {
				view_set_untiled(view);
				view_apply_natural_geometry(view);
				break;
			}
			bool combine = action_get_bool(action, "combine", false);
			view_snap_to_edge(view, edge, /*across_outputs*/ true,
				combine, /*store_natural_geometry*/ true);
		}
		break;
	case ACTION_TYPE_GROW_TO_EDGE:
		if (view) {
			/* Config parsing makes sure that direction is a valid direction */
			enum lab_edge edge = action_get_int(action, "direction", 0);
			view_grow_to_edge(view, edge);
		}
		break;
	case ACTION_TYPE_SHRINK_TO_EDGE:
		if (view) {
			/* Config parsing makes sure that direction is a valid direction */
			enum lab_edge edge = action_get_int(action, "direction", 0);
			view_shrink_to_edge(view, edge);
		}
		break;
	case ACTION_TYPE_NEXT_WINDOW:
		if (server->input_mode == LAB_INPUT_STATE_CYCLE) {
			cycle_step(server, LAB_CYCLE_DIR_FORWARD);
		} else {
			cycle_begin(server, LAB_CYCLE_DIR_FORWARD);
		}
		break;
	case ACTION_TYPE_PREVIOUS_WINDOW:
		if (server->input_mode == LAB_INPUT_STATE_CYCLE) {
			cycle_step(server, LAB_CYCLE_DIR_BACKWARD);
		} else {
			cycle_begin(server, LAB_CYCLE_DIR_BACKWARD);
		}
		break;
	case ACTION_TYPE_RECONFIGURE:
		kill(getpid(), SIGHUP);
		break;
	case ACTION_TYPE_SHOW_MENU:
		show_menu(server, view, ctx,
			action_get_str(action, "menu", NULL),
			action_get_bool(action, "atCursor", true),
			action_get_str(action, "x.position", NULL),
			action_get_str(action, "y.position", NULL));
		break;
	case ACTION_TYPE_TOGGLE_MAXIMIZE:
		if (view) {
			enum view_axis axis = action_get_int(action,
				"direction", VIEW_AXIS_BOTH);
			view_toggle_maximize(view, axis);
		}
		break;
	case ACTION_TYPE_MAXIMIZE:
		if (view) {
			enum view_axis axis = action_get_int(action,
				"direction", VIEW_AXIS_BOTH);
			view_maximize(view, axis,
				/*store_natural_geometry*/ true);
		}
		break;
	case ACTION_TYPE_UNMAXIMIZE:
		if (view) {
			enum view_axis axis = action_get_int(action,
				"direction", VIEW_AXIS_BOTH);
			view_maximize(view, view->maximized & ~axis,
				/*store_natural_geometry*/ true);
		}
		break;
	case ACTION_TYPE_TOGGLE_FULLSCREEN:
		if (view) {
			view_toggle_fullscreen(view);
		}
		break;
	case ACTION_TYPE_SET_DECORATIONS:
		if (view) {
			enum lab_ssd_mode mode = action_get_int(action,
				"decorations", LAB_SSD_MODE_FULL);
			bool force_ssd = action_get_bool(action,
				"forceSSD", false);
			view_set_decorations(view, mode, force_ssd);
		}
		break;
	case ACTION_TYPE_TOGGLE_DECORATIONS:
		if (view) {
			view_toggle_decorations(view);
		}
		break;
	case ACTION_TYPE_TOGGLE_ALWAYS_ON_TOP:
		if (view) {
			view_toggle_always_on_top(view);
		}
		break;
	case ACTION_TYPE_TOGGLE_ALWAYS_ON_BOTTOM:
		if (view) {
			view_toggle_always_on_bottom(view);
		}
		break;
	case ACTION_TYPE_TOGGLE_OMNIPRESENT:
		if (view) {
			view_toggle_visible_on_all_workspaces(view);
		}
		break;
	case ACTION_TYPE_FOCUS:
		if (view) {
			desktop_focus_view(view, /*raise*/ false);
		}
		break;
	case ACTION_TYPE_UNFOCUS:
		seat_focus_surface(&server->seat, NULL);
		break;
	case ACTION_TYPE_ICONIFY:
		if (view) {
			view_minimize(view, true);
		}
		break;
	case ACTION_TYPE_MOVE:
		if (view) {
			interactive_begin(view, LAB_INPUT_STATE_MOVE,
				LAB_EDGE_NONE);
		}
		break;
	case ACTION_TYPE_RAISE:
		if (view) {
			view_move_to_front(view);
		}
		break;
	case ACTION_TYPE_LOWER:
		if (view) {
			view_move_to_back(view);
		}
		break;
	case ACTION_TYPE_RESIZE:
		if (view) {
			/*
			 * If a direction was specified in the config, honour it.
			 * Otherwise, fall back to determining the resize edges from
			 * the current cursor position (existing behaviour).
			 */
			enum lab_edge resize_edges =
				action_get_int(action, "direction", LAB_EDGE_NONE);
			if (resize_edges == LAB_EDGE_NONE) {
				resize_edges = cursor_get_resize_edges(
					server->seat.cursor, ctx);
			}
			interactive_begin(view, LAB_INPUT_STATE_RESIZE,
				resize_edges);
		}
		break;
	case ACTION_TYPE_RESIZE_RELATIVE:
		if (view) {
			int left = action_get_int(action, "left", 0);
			int right = action_get_int(action, "right", 0);
			int top = action_get_int(action, "top", 0);
			int bottom = action_get_int(action, "bottom", 0);
			view_resize_relative(view, left, right, top, bottom);
		}
		break;
	case ACTION_TYPE_MOVETO:
		if (view) {
			int x = action_get_int(action, "x", 0);
			int y = action_get_int(action, "y", 0);
			struct border margin = ssd_thickness(view);
			view_move(view, x + margin.left, y + margin.top);
		}
		break;
	case ACTION_TYPE_RESIZETO:
		if (view) {
			int width = action_get_int(action, "width", 0);
			int height = action_get_int(action, "height", 0);

			/*
			 * To support only setting one of width/height
			 * in <action name="ResizeTo" width="" height=""/>
			 * we fall back to current dimension when unset.
			 */
			struct wlr_box box = {
				.x = view->pending.x,
				.y = view->pending.y,
				.width = width ? : view->pending.width,
				.height = height ? : view->pending.height,
			};
			view_set_shade(view, false);
			view_move_resize(view, box);
		}
		break;
	case ACTION_TYPE_MOVE_RELATIVE:
		if (view) {
			int x = action_get_int(action, "x", 0);
			int y = action_get_int(action, "y", 0);
			view_move_relative(view, x, y);
		}
		break;
	case ACTION_TYPE_MOVETO_CURSOR:
		wlr_log(WLR_ERROR,
			"Action MoveToCursor is deprecated. To ensure your config works in future labwc "
			"releases, please use <action name=\"AutoPlace\" policy=\"cursor\">");
		if (view) {
			view_move_to_cursor(view);
		}
		break;
	case ACTION_TYPE_SEND_TO_DESKTOP:
		if (!view) {
			break;
		}
		/* Falls through to GoToDesktop */
	case ACTION_TYPE_GO_TO_DESKTOP: {
		bool follow = true;
		bool wrap = action_get_bool(action, "wrap", true);
		const char *to = action_get_str(action, "to", NULL);
		/*
		 * `to` is always != NULL here because otherwise we would have
		 * removed the action during the initial parsing step as it is
		 * a required argument for both SendToDesktop and GoToDesktop.
		 */
		struct workspace *target_workspace = workspaces_find(
			server->workspaces.current, to, wrap);
		if (action->type == ACTION_TYPE_GO_TO_DESKTOP) {
			bool toggle = action_get_bool(action, "toggle", false);
			if (target_workspace == server->workspaces.current
				&& toggle) {
				target_workspace = server->workspaces.last;
			}
		}
		if (!target_workspace) {
			break;
		}
		if (action->type == ACTION_TYPE_SEND_TO_DESKTOP) {
			view_move_to_workspace(view, target_workspace);
			follow = action_get_bool(action, "follow", true);

			/* Ensure that the focus is not on another desktop */
			if (!follow && server->active_view == view) {
				desktop_focus_topmost_view(server);
			}
		}
		if (follow) {
			workspaces_switch_to(target_workspace,
				/*update_focus*/ true);
		}
		break;
	}
	case ACTION_TYPE_MOVE_TO_OUTPUT: {
		if (!view) {
			break;
		}
		struct output *target_output =
			get_target_output(view->output, server, action);
		if (target_output) {
			view_move_to_output(view, target_output);
		}
		break;
	}
	case ACTION_TYPE_FIT_TO_OUTPUT:
		if (!view) {
			break;
		}
		view_constrain_size_to_that_of_usable_area(view);
		break;
	case ACTION_TYPE_TOGGLE_SNAP_TO_REGION:
	case ACTION_TYPE_SNAP_TO_REGION: {
		if (!view) {
			break;
		}
		struct output *output = view->output;
		if (!output) {
			break;
		}
		const char *region_name = action_get_str(action, "region", NULL);
		struct region *region = regions_from_name(region_name, output);
		if (region) {
			if (action->type == ACTION_TYPE_TOGGLE_SNAP_TO_REGION
					&& view->maximized == VIEW_AXIS_NONE
					&& !view->fullscreen
					&& view_is_tiled(view)
					&& view->tiled_region == region) {
				view_set_untiled(view);
				view_apply_natural_geometry(view);
				break;
			}
			view_snap_to_region(view, region,
				/*store_natural_geometry*/ true);
		} else {
			wlr_log(WLR_ERROR, "Invalid SnapToRegion id: '%s'", region_name);
		}
		break;
	}
	case ACTION_TYPE_UNSNAP:
		if (view && !view->fullscreen && !view_is_floating(view)) {
			view_maximize(view, VIEW_AXIS_NONE,
				/* store_natural_geometry */ false);
			view_set_untiled(view);
			view_apply_natural_geometry(view);
		}
		break;
	case ACTION_TYPE_TOGGLE_KEYBINDS:
		if (view) {
			view_toggle_keybinds(view);
		}
		break;
	case ACTION_TYPE_FOCUS_OUTPUT: {
		struct output *output = output_nearest_to_cursor(server);
		struct output *target_output =
			get_target_output(output, server, action);
		if (target_output) {
			desktop_focus_output(target_output);
		}
		break;
	}
	case ACTION_TYPE_IF: {
		/* At least one of the queries was matched or there was no query */
		if (action_get_str(action, "message.prompt", NULL)) {
			/*
			 * We delay the selection and execution of the
			 * branch until we get a response from the user.
			 */
			action_prompt_create(view, server, action);
		} else if (view) {
			struct wl_list *actions;
			if (match_queries(view, action)) {
				actions = action_get_actionlist(action, "then");
			} else {
				actions = action_get_actionlist(action, "else");
			}
			if (actions) {
				actions_run(view, server, actions, ctx);
			}
		}
		break;
	}
	case ACTION_TYPE_FOR_EACH: {
		struct wl_list *actions = NULL;
		bool matches = false;

		struct wl_array views;
		wl_array_init(&views);
		view_array_append(server, &views, LAB_VIEW_CRITERIA_NONE);

		struct view **item;
		wl_array_for_each(item, &views) {
			if (match_queries(*item, action)) {
				matches = true;
				actions = action_get_actionlist(action, "then");
			} else {
				actions = action_get_actionlist(action, "else");
			}
			if (actions) {
				actions_run(*item, server, actions, ctx);
			}
		}
		wl_array_release(&views);
		if (!matches) {
			actions = action_get_actionlist(action, "none");
			if (actions) {
				actions_run(view, server, actions, NULL);
			}
		}
		break;
	}
	case ACTION_TYPE_VIRTUAL_OUTPUT_ADD: {
		/* TODO: rename this argument to "outputName" */
		const char *output_name =
			action_get_str(action, "output_name", NULL);
		output_virtual_add(server, output_name,
				/*store_wlr_output*/ NULL);
		break;
	}
	case ACTION_TYPE_VIRTUAL_OUTPUT_REMOVE: {
		/* TODO: rename this argument to "outputName" */
		const char *output_name =
			action_get_str(action, "output_name", NULL);
		output_virtual_remove(server, output_name);
		break;
	}
	case ACTION_TYPE_AUTO_PLACE:
		if (view) {
			enum lab_placement_policy policy =
				action_get_int(action, "policy", LAB_PLACE_AUTOMATIC);
			view_place_by_policy(view,
				/* allow_cursor */ true, policy);
		}
		break;
	case ACTION_TYPE_TOGGLE_TEARING:
		if (view) {
			switch (view->force_tearing) {
			case LAB_STATE_UNSPECIFIED:
				view->force_tearing =
					output_get_tearing_allowance(view->output)
						? LAB_STATE_DISABLED : LAB_STATE_ENABLED;
				break;
			case LAB_STATE_DISABLED:
				view->force_tearing = LAB_STATE_ENABLED;
				break;
			case LAB_STATE_ENABLED:
				view->force_tearing = LAB_STATE_DISABLED;
				break;
			}
			wlr_log(WLR_ERROR, "force tearing %sabled",
				view->force_tearing == LAB_STATE_ENABLED
					? "en" : "dis");
		}
		break;
	case ACTION_TYPE_TOGGLE_SHADE:
		if (view) {
			view_set_shade(view, !view->shaded);
		}
		break;
	case ACTION_TYPE_SHADE:
		if (view) {
			view_set_shade(view, true);
		}
		break;
	case ACTION_TYPE_UNSHADE:
		if (view) {
			view_set_shade(view, false);
		}
		break;
	case ACTION_TYPE_ENABLE_SCROLL_WHEEL_EMULATION:
		server->seat.cursor_scroll_wheel_emulation = true;
		break;
	case ACTION_TYPE_DISABLE_SCROLL_WHEEL_EMULATION:
		server->seat.cursor_scroll_wheel_emulation = false;
		break;
	case ACTION_TYPE_TOGGLE_SCROLL_WHEEL_EMULATION:
		server->seat.cursor_scroll_wheel_emulation =
			!server->seat.cursor_scroll_wheel_emulation;
		break;
	case ACTION_TYPE_ENABLE_TABLET_MOUSE_EMULATION:
		rc.tablet.force_mouse_emulation = true;
		break;
	case ACTION_TYPE_DISABLE_TABLET_MOUSE_EMULATION:
		rc.tablet.force_mouse_emulation = false;
		break;
	case ACTION_TYPE_TOGGLE_TABLET_MOUSE_EMULATION:
		rc.tablet.force_mouse_emulation = !rc.tablet.force_mouse_emulation;
		break;
	case ACTION_TYPE_TOGGLE_MAGNIFY:
		magnifier_toggle(server);
		break;
	case ACTION_TYPE_ZOOM_IN:
		magnifier_set_scale(server, MAGNIFY_INCREASE);
		break;
	case ACTION_TYPE_ZOOM_OUT:
		magnifier_set_scale(server, MAGNIFY_DECREASE);
		break;
	case ACTION_TYPE_WARP_CURSOR: {
		const char *to = action_get_str(action, "to", "output");
		const char *x = action_get_str(action, "x", "center");
		const char *y = action_get_str(action, "y", "center");
		warp_cursor(server, view, to, x, y);
		break;
	}
	case ACTION_TYPE_HIDE_CURSOR:
		cursor_set_visible(&server->seat, false);
		break;
	case ACTION_TYPE_INVALID:
		wlr_log(WLR_ERROR, "Not executing unknown action");
		break;
	default:
		/*
		 * If we get here it must be a BUG caused most likely by
		 * action_names and action_type being out of sync or by
		 * adding a new action without installing a handler here.
		 */
		wlr_log(WLR_ERROR,
			"Not executing invalid action (%u)"
			" This is a BUG. Please report.", action->type);
	}
}

void
actions_run(struct view *activator, struct server *server,
	struct wl_list *actions, struct cursor_context *cursor_ctx)
{
	if (!actions) {
		wlr_log(WLR_ERROR, "empty actions");
		return;
	}

	/* This cancels any pending on-release keybinds */
	keyboard_reset_current_keybind();

	struct cursor_context ctx = {0};
	if (cursor_ctx) {
		ctx = *cursor_ctx;
	}

	struct action *action;
	wl_list_for_each(action, actions, link) {
		if (server->input_mode == LAB_INPUT_STATE_CYCLE
				&& action->type != ACTION_TYPE_NEXT_WINDOW
				&& action->type != ACTION_TYPE_PREVIOUS_WINDOW) {
			wlr_log(WLR_INFO, "Only NextWindow or PreviousWindow "
				"actions are accepted while window switching.");
			continue;
		}

		wlr_log(WLR_DEBUG, "Handling action %u: %s", action->type,
			action_names[action->type]);

		/*
		 * Refetch view because it may have been changed due to the
		 * previous action
		 */
		struct view *view = view_for_action(activator, server, action, &ctx);

		run_action(view, server, action, &ctx);
	}
}
