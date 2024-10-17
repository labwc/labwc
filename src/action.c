// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/macros.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "debug.h"
#include "labwc.h"
#include "magnifier.h"
#include "menu/menu.h"
#include "osd.h"
#include "output-virtual.h"
#include "regions.h"
#include "ssd.h"
#include "view.h"
#include "workspaces.h"
#include "input/keyboard.h"

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
	ACTION_TYPE_ENABLE_TABLET_MOUSE_EMULATION,
	ACTION_TYPE_DISABLE_TABLET_MOUSE_EMULATION,
	ACTION_TYPE_TOGGLE_TABLET_MOUSE_EMULATION,
	ACTION_TYPE_TOGGLE_MAGNIFY,
	ACTION_TYPE_ZOOM_IN,
	ACTION_TYPE_ZOOM_OUT
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
	"EnableTabletMouseEmulation",
	"DisableTabletMouseEmulation",
	"ToggleTabletMouseEmulation",
	"ToggleMagnify",
	"ZoomIn",
	"ZoomOut",
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

static const char *
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
		if (!strcasecmp(argument, "snapWindows")) {
			action_arg_add_bool(action, argument, parse_bool(content, true));
			goto cleanup;
		}
		/* Falls through */
	case ACTION_TYPE_TOGGLE_SNAP_TO_EDGE:
	case ACTION_TYPE_SNAP_TO_EDGE:
	case ACTION_TYPE_GROW_TO_EDGE:
	case ACTION_TYPE_SHRINK_TO_EDGE:
		if (!strcmp(argument, "direction")) {
			enum view_edge edge = view_edge_parse(content);
			bool allow_center = action->type == ACTION_TYPE_TOGGLE_SNAP_TO_EDGE
				|| action->type == ACTION_TYPE_SNAP_TO_EDGE;
			if ((edge == VIEW_EDGE_CENTER && !allow_center)
					|| edge == VIEW_EDGE_INVALID) {
				wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s' (%s)",
					action_names[action->type], argument, content);
			} else {
				action_arg_add_int(action, argument, edge);
			}
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
			enum ssd_mode mode = ssd_mode_parse(content);
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
			enum view_edge edge = view_edge_parse(content);
			if (edge == VIEW_EDGE_CENTER) {
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
			enum view_placement_policy policy =
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
		; /* works around "a label can only be part of a statement" */
		static const char * const branches[] = { "then", "else", "none" };
		for (size_t i = 0; i < ARRAY_SIZE(branches); i++) {
			struct wl_list *children = action_get_actionlist(action, branches[i]);
			if (children && !action_list_is_valid(children)) {
				wlr_log(WLR_ERROR, "Invalid action in %s '%s' branch",
					action_names[action->type], branches[i]);
				return false;
			}
		}
		return true;
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

	/*
	 * We always refresh client-list-combined-menu and client-send-to-menu
	 * so that they are up-to-date whether they are directly opened as a
	 * top-level menu or opened as a submenu which we don't know at this
	 * point. It is also needed to calculate the proper width for placement
	 * as it fluctuates depending on application/workspace titles.
	 */
	update_client_list_combined_menu(menu->server);
	update_client_send_to_menu(menu->server);

	int x = server->seat.cursor->x;
	int y = server->seat.cursor->y;

	/* The client menu needs an active client */
	bool is_client_menu = !strcasecmp(menu_name, "client-menu");
	if (is_client_menu && !view) {
		return;
	}
	/* Place menu in the view corner if desired (and menu is not root-menu) */
	if (!at_cursor && view) {
		x = view->current.x;
		y = view->current.y;
		/* Push the client menu underneath the button */
		if (is_client_menu && ssd_part_contains(
				LAB_SSD_BUTTON, ctx->type)) {
			assert(ctx->node);
			int ly;
			wlr_scene_node_coords(ctx->node, &x, &ly);
		}
	}

	/*
	 * determine placement by looking at x and y
	 * x/y can be number, "center" or a %percent of screen dimensions
	 */
	if (pos_x && pos_y) {
		struct output *output = output_nearest_to(server,
				server->seat.cursor->x, server->seat.cursor->y);
		struct menuitem *item;
		struct theme *theme = server->theme;
		int max_width = theme->menu_min_width;

		wl_list_for_each(item, &menu->menuitems, link) {
			if (item->native_width > max_width) {
				max_width = item->native_width < theme->menu_max_width
					? item->native_width : theme->menu_max_width;
			}
		}

		if (!strcasecmp(pos_x, "center")) {
			x = (output->usable_area.width / 2) - (max_width / 2);
		} else if (strchr(pos_x, '%')) {
			x = (output->usable_area.width * atoi(pos_x)) / 100;
		} else {
			if (pos_x[0] == '-') {
				int neg_x = strtol(pos_x, NULL, 10);
				x = output->usable_area.width + neg_x;
			} else {
				x = atoi(pos_x);
			}
		}

		if (!strcasecmp(pos_y, "center")) {
			y = (output->usable_area.height / 2) - (menu->size.height / 2);
		} else if (strchr(pos_y, '%')) {
			y = (output->usable_area.height * atoi(pos_y)) / 100;
		} else {
			if (pos_y[0] == '-') {
				int neg_y = strtol(pos_y, NULL, 10);
				y = output->usable_area.height + neg_y;
			} else {
				y = atoi(pos_y);
			}
		}
		/* keep menu from being off screen */
		x = MAX(x, 0);
		x = MIN(x, (output->usable_area.width -1));
		y = MAX(y, 0);
		y = MIN(y, (output->usable_area.height -1));
		/* adjust for which monitor to appear on */
		x += output->usable_area.x;
		y += output->usable_area.y;
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

static bool
run_if_action(struct view *view, struct server *server, struct action *action)
{
	struct view_query *query;
	struct wl_list *queries, *actions;
	const char *branch = "then";

	queries = action_get_querylist(action, "query");
	if (queries) {
		branch = "else";
		/* All queries are OR'ed */
		wl_list_for_each(query, queries, link) {
			if (view_matches_query(view, query)) {
				branch = "then";
				break;
			}
		}
	}

	actions = action_get_actionlist(action, branch);
	if (actions) {
		actions_run(view, server, actions, NULL);
	}
	return !strcmp(branch, "then");
}

static bool
shift_is_pressed(struct server *server)
{
	uint32_t modifiers = wlr_keyboard_get_modifiers(
			&server->seat.keyboard_group->keyboard);
	return modifiers & WLR_MODIFIER_SHIFT;
}

static void
start_window_cycling(struct server *server, enum lab_cycle_dir direction)
{
	/* Remember direction so it can be followed by subsequent key presses */
	server->osd_state.initial_direction = direction;
	server->osd_state.initial_keybind_contained_shift =
		shift_is_pressed(server);
	server->osd_state.cycle_view = desktop_cycle_view(server,
		server->osd_state.cycle_view, direction);
	osd_update(server);
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
		enum view_edge edge =
			action_get_int(action, "direction", VIEW_EDGE_INVALID);
		bool wrap = action_get_bool(action, "wrap", false);
		target = output_get_adjacent(output, edge, wrap);
	}

	if (!target) {
		wlr_log(WLR_DEBUG, "Invalid output");
	}

	return target;
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

	struct view *view;
	struct action *action;
	struct output *output;
	struct output *target;
	struct cursor_context ctx = {0};
	if (cursor_ctx) {
		ctx = *cursor_ctx;
	}

	wl_list_for_each(action, actions, link) {
		wlr_log(WLR_DEBUG, "Handling action %u: %s", action->type,
			action_names[action->type]);

		/*
		 * Refetch view because it may have been changed due to the
		 * previous action
		 */
		view = view_for_action(activator, server, action, &ctx);

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
		case ACTION_TYPE_EXECUTE:
			{
				struct buf cmd = BUF_INIT;
				buf_add(&cmd, action_get_str(action, "command", NULL));
				buf_expand_tilde(&cmd);
				spawn_async_no_shell(cmd.data);
				buf_reset(&cmd);
			}
			break;
		case ACTION_TYPE_EXIT:
			wl_display_terminate(server->wl_display);
			break;
		case ACTION_TYPE_MOVE_TO_EDGE:
			if (view) {
				/* Config parsing makes sure that direction is a valid direction */
				enum view_edge edge = action_get_int(action, "direction", 0);
				bool snap_to_windows = action_get_bool(action, "snapWindows", true);
				view_move_to_edge(view, edge, snap_to_windows);
			}
			break;
		case ACTION_TYPE_TOGGLE_SNAP_TO_EDGE:
		case ACTION_TYPE_SNAP_TO_EDGE:
			if (view) {
				/* Config parsing makes sure that direction is a valid direction */
				enum view_edge edge = action_get_int(action, "direction", 0);
				if (action->type == ACTION_TYPE_TOGGLE_SNAP_TO_EDGE
						&& view->maximized == VIEW_AXIS_NONE
						&& !view->fullscreen
						&& view_is_tiled(view)
						&& view->tiled == edge) {
					view_set_untiled(view);
					view_apply_natural_geometry(view);
					break;
				}
				view_snap_to_edge(view, edge,
					/*across_outputs*/ true,
					/*store_natural_geometry*/ true);
			}
			break;
		case ACTION_TYPE_GROW_TO_EDGE:
			if (view) {
				/* Config parsing makes sure that direction is a valid direction */
				enum view_edge edge = action_get_int(action, "direction", 0);
				view_grow_to_edge(view, edge);
			}
			break;
		case ACTION_TYPE_SHRINK_TO_EDGE:
			if (view) {
				/* Config parsing makes sure that direction is a valid direction */
				enum view_edge edge = action_get_int(action, "direction", 0);
				view_shrink_to_edge(view, edge);
			}
			break;
		case ACTION_TYPE_NEXT_WINDOW:
			start_window_cycling(server, LAB_CYCLE_DIR_FORWARD);
			break;
		case ACTION_TYPE_PREVIOUS_WINDOW:
			start_window_cycling(server, LAB_CYCLE_DIR_BACKWARD);
			break;
		case ACTION_TYPE_RECONFIGURE:
			kill(getpid(), SIGHUP);
			break;
		case ACTION_TYPE_SHOW_MENU:
			show_menu(server, view, &ctx,
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
				enum ssd_mode mode = action_get_int(action,
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
				interactive_begin(view, LAB_INPUT_STATE_MOVE, 0);
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
				uint32_t resize_edges = cursor_get_resize_edges(
					server->seat.cursor, &ctx);
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
				view_move(view, x, y);
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
		case ACTION_TYPE_GO_TO_DESKTOP:
			{
				bool follow = true;
				bool wrap = action_get_bool(action, "wrap", true);
				const char *to = action_get_str(action, "to", NULL);
				/*
				 * `to` is always != NULL here because otherwise we would have
				 * removed the action during the initial parsing step as it is
				 * a required argument for both SendToDesktop and GoToDesktop.
				 */
				struct workspace *target = workspaces_find(
					server->workspaces.current, to, wrap);
				if (!target) {
					break;
				}
				if (action->type == ACTION_TYPE_SEND_TO_DESKTOP) {
					view_move_to_workspace(view, target);
					follow = action_get_bool(action, "follow", true);

					/* Ensure that the focus is not on another desktop */
					if (!follow && server->active_view == view) {
						desktop_focus_topmost_view(server);
					}
				}
				if (follow) {
					workspaces_switch_to(target,
						/*update_focus*/ true);
				}
			}
			break;
		case ACTION_TYPE_MOVE_TO_OUTPUT:
			if (!view) {
				break;
			}
			target = get_target_output(view->output, server, action);
			if (target) {
				view_move_to_output(view, target);
			}
			break;
		case ACTION_TYPE_FIT_TO_OUTPUT:
			if (!view) {
				break;
			}
			view_constrain_size_to_that_of_usable_area(view);
			break;
		case ACTION_TYPE_TOGGLE_SNAP_TO_REGION:
		case ACTION_TYPE_SNAP_TO_REGION:
			if (!view) {
				break;
			}
			output = view->output;
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
		case ACTION_TYPE_FOCUS_OUTPUT:
			output = output_nearest_to_cursor(server);
			target = get_target_output(output, server, action);
			if (target) {
				desktop_focus_output(target);
			}
			break;
		case ACTION_TYPE_IF:
			if (view) {
				run_if_action(view, server, action);
			}
			break;
		case ACTION_TYPE_FOR_EACH:
			{
				struct wl_array views;
				struct view **item;
				bool matches = false;
				wl_array_init(&views);
				view_array_append(server, &views, LAB_VIEW_CRITERIA_NONE);
				wl_array_for_each(item, &views) {
					matches |= run_if_action(*item, server, action);
				}
				wl_array_release(&views);
				if (!matches) {
					struct wl_list *actions;
					actions = action_get_actionlist(action, "none");
					if (actions) {
						actions_run(view, server, actions, NULL);
					}
				}
			}
			break;
		case ACTION_TYPE_VIRTUAL_OUTPUT_ADD:
			{
				const char *output_name = action_get_str(action, "output_name",
						NULL);
				output_virtual_add(server, output_name,
					/*store_wlr_output*/ NULL);
			}
			break;
		case ACTION_TYPE_VIRTUAL_OUTPUT_REMOVE:
			{
				const char *output_name = action_get_str(action, "output_name",
						NULL);
				output_virtual_remove(server, output_name);
			}
			break;
		case ACTION_TYPE_AUTO_PLACE:
			if (view) {
				enum view_placement_policy policy =
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
			magnify_toggle(server);
			break;
		case ACTION_TYPE_ZOOM_IN:
			magnify_set_scale(server, MAGNIFY_INCREASE);
			break;
		case ACTION_TYPE_ZOOM_OUT:
			magnify_set_scale(server, MAGNIFY_DECREASE);
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
}
