// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <wayland-util.h>
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
#include "menu/menu.h"
#include "osd.h"
#include "output-virtual.h"
#include "placement.h"
#include "regions.h"
#include "ssd.h"
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
	ACTION_TYPE_SNAP_TO_EDGE,
	ACTION_TYPE_GROW_TO_EDGE,
	ACTION_TYPE_SHRINK_TO_EDGE,
	ACTION_TYPE_DIRECTIONAL_TARGET_WINDOW,
	ACTION_TYPE_NEXT_WINDOW,
	ACTION_TYPE_PREVIOUS_WINDOW,
	ACTION_TYPE_RECONFIGURE,
	ACTION_TYPE_SHOW_MENU,
	ACTION_TYPE_TOGGLE_MAXIMIZE,
	ACTION_TYPE_MAXIMIZE,
	ACTION_TYPE_TOGGLE_FULLSCREEN,
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
	ACTION_TYPE_SNAP_TO_REGION,
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
	"SnapToEdge",
	"GrowToEdge",
	"ShrinkToEdge",
	"DirectionalTargetWindow",
	"NextWindow",
	"PreviousWindow",
	"Reconfigure",
	"ShowMenu",
	"ToggleMaximize",
	"Maximize",
	"ToggleFullscreen",
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
	"SnapToRegion",
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
	case ACTION_TYPE_SNAP_TO_EDGE:
	case ACTION_TYPE_GROW_TO_EDGE:
	case ACTION_TYPE_DIRECTIONAL_TARGET_WINDOW:
	case ACTION_TYPE_SHRINK_TO_EDGE:
		if (!strcmp(argument, "direction")) {
			enum view_edge edge = view_edge_parse(content);
			if ((edge == VIEW_EDGE_CENTER && action->type != ACTION_TYPE_SNAP_TO_EDGE)
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
		break;
	case ACTION_TYPE_TOGGLE_MAXIMIZE:
	case ACTION_TYPE_MAXIMIZE:
		if (!strcmp(argument, "direction")) {
			enum view_axis axis = view_axis_parse(content);
			if (axis == VIEW_AXIS_NONE) {
				wlr_log(WLR_ERROR, "Invalid argument for action %s: '%s' (%s)",
					action_names[action->type], argument, content);
			} else {
				action_arg_add_int(action, argument, axis);
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
		break;
	case ACTION_TYPE_SNAP_TO_REGION:
		if (!strcmp(argument, "region")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_FOCUS_OUTPUT:
		if (!strcmp(argument, "output")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
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
	case ACTION_TYPE_DIRECTIONAL_TARGET_WINDOW:
	case ACTION_TYPE_MOVE_TO_EDGE:
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
	case ACTION_TYPE_SNAP_TO_REGION:
		arg_name = "region";
		break;
	case ACTION_TYPE_FOCUS_OUTPUT:
		arg_name = "output";
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
show_menu(struct server *server, struct view *view,
		const char *menu_name, bool at_cursor)
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
	if (!view && strcasecmp(menu_name, "client-menu") == 0) {
		return;
	}

	/* Place menu in the view corner if desired (and menu is not root-menu) */
	if (!at_cursor && view) {
		x = view->current.x;
		y = view->current.y;
	}

	/* Replaced by next show_menu() or cleaned on view_destroy() */
	menu->triggered_by_view = view;
	menu_open_root(menu, x, y);
}

static struct view *
view_for_action(struct view *activator, struct server *server,
	struct action *action, uint32_t *resize_edges)
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
		struct cursor_context ctx = get_cursor_context(server);
		if (action->type == ACTION_TYPE_RESIZE) {
			/* Select resize edges for the keybind case */
			*resize_edges = cursor_get_resize_edges(
				server->seat.cursor, &ctx);
		}
		return ctx.view;
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
		actions_run(view, server, actions, 0);
	}
	return !strcmp(branch, "then");
}

static struct view*
directional_target_window(struct view *view, struct server *server,
		enum view_edge direction, bool wrap)
{
	int dx, dy, distance, distance_wrap;
	struct view *v;
	struct output *v_output;
	struct wlr_box v_usable;
	struct view *closest_view = NULL;
	struct view *closest_view_wrap = NULL;
	int min_distance = INT_MAX;
	int min_distance_wrap = INT_MAX;
	struct output *output = view->output;
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	int cx = view->current.x + view->current.width / 2;
	int cy = view->current.y + view->current.height / 2;
	assert(view);
	for_each_view(v, &server->views,
			LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v->minimized) {
			continue;
		}
		v_output = v->output;
		v_usable = output_usable_area_in_layout_coords(v_output);
		dx = v->current.x + v->current.width/2 - cx;
		dy = v->current.y + v->current.height/2 - cy;
		distance = dx * dx + dy * dy;
		distance_wrap = INT_MAX;
		switch (direction) {
		case VIEW_EDGE_LEFT:
			if (dx == 0) {
				continue;
			}
			if (dx > 0) {
				distance = INT_MAX;
				dx = v_usable.x + v_usable.width - dx;
				distance_wrap = dx * dx + dy * dy;
			}
			break;
		case VIEW_EDGE_RIGHT:
			if (dx == 0) {
				continue;
			}
			if (dx < 0) {
				distance = INT_MAX;
				dx = usable.x + usable.width + dx;
				distance_wrap = dx * dx + dy * dy;
			}
			break;
		case VIEW_EDGE_UP:
			if (dy == 0) {
				continue;
			}
			if (dy > 0) {
				distance = INT_MAX;
				dy = v_usable.y + v_usable.height - dy;
				distance_wrap = dx * dx + dy * dy;
			}
			break;
		case VIEW_EDGE_DOWN:
			if (dy == 0) {
				continue;
			}
			if (dy < 0) {
				distance = INT_MAX;
				dy = usable.y + usable.height + dy;
				distance_wrap = dx * dx + dy * dy;
			}
			break;
		default:
		return NULL;
		}
		if (distance < min_distance) {
			min_distance = distance;
			closest_view = v;
		} else if (distance_wrap < min_distance_wrap) {
			min_distance_wrap = distance_wrap;
			closest_view_wrap = v;
		}
	}
	if (closest_view) {
		return closest_view;
	}
	if (closest_view_wrap && wrap) {
		return closest_view_wrap;
	}
	return NULL;
}

void
actions_run(struct view *activator, struct server *server,
	struct wl_list *actions, uint32_t resize_edges)
{
	if (!actions) {
		wlr_log(WLR_ERROR, "empty actions");
		return;
	}

	struct view *view;
	struct action *action;
	wl_list_for_each(action, actions, link) {
		wlr_log(WLR_DEBUG, "Handling action %u: %s", action->type,
			action_names[action->type]);

		/*
		 * Refetch view because it may have been changed due to the
		 * previous action
		 */
		view = view_for_action(activator, server, action,
			&resize_edges);

		switch (action->type) {
		case ACTION_TYPE_CLOSE:
			if (view) {
				view_close(view);
			}
			break;
		case ACTION_TYPE_KILL:
			if (view && view->surface) {
				/* Send SIGTERM to the process associated with the surface */
				pid_t pid = -1;
				struct wl_client *client = view->surface->resource->client;
				wl_client_get_credentials(client, &pid, NULL, NULL);
				if (pid != -1) {
					kill(pid, SIGTERM);
				}
			}
			break;
		case ACTION_TYPE_DEBUG:
			debug_dump_scene(server);
			break;
		case ACTION_TYPE_EXECUTE:
			{
				struct buf cmd;
				buf_init(&cmd);
				buf_add(&cmd, action_get_str(action, "command", NULL));
				buf_expand_tilde(&cmd);
				spawn_async_no_shell(cmd.buf);
				free(cmd.buf);
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
		case ACTION_TYPE_SNAP_TO_EDGE:
			if (view) {
				/* Config parsing makes sure that direction is a valid direction */
				enum view_edge edge = action_get_int(action, "direction", 0);
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
		case ACTION_TYPE_DIRECTIONAL_TARGET_WINDOW:
			if (view) {
				enum view_edge direction = action_get_int(action, "direction", 0);
				struct view *closest_view = directional_target_window(view, server,
						direction, /*wrap*/ true);
				if (closest_view) {
					desktop_focus_view(closest_view, /*raise*/ true);
				}
			}
			break;
		case ACTION_TYPE_NEXT_WINDOW:
			server->osd_state.cycle_view = desktop_cycle_view(server,
				server->osd_state.cycle_view, LAB_CYCLE_DIR_FORWARD);
			osd_update(server);
			break;
		case ACTION_TYPE_PREVIOUS_WINDOW:
			server->osd_state.cycle_view = desktop_cycle_view(server,
				server->osd_state.cycle_view, LAB_CYCLE_DIR_BACKWARD);
			osd_update(server);
			break;
		case ACTION_TYPE_RECONFIGURE:
			kill(getpid(), SIGHUP);
			break;
		case ACTION_TYPE_SHOW_MENU:
			show_menu(server, view,
				action_get_str(action, "menu", NULL),
				action_get_bool(action, "atCursor", true));
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
		case ACTION_TYPE_TOGGLE_FULLSCREEN:
			if (view) {
				view_toggle_fullscreen(view);
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
					server->workspace_current, to, wrap);
				if (!target) {
					break;
				}
				if (action->type == ACTION_TYPE_SEND_TO_DESKTOP) {
					view_move_to_workspace(view, target);
					follow = action_get_bool(action, "follow", true);
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
			const char *output_name = action_get_str(action, "output", NULL);
			struct output *target = NULL;
			if (output_name) {
				target = output_from_name(view->server, output_name);
			} else {
				enum view_edge edge = action_get_int(action, "direction", 0);
				bool wrap = action_get_bool(action, "wrap", false);
				target = view_get_adjacent_output(view, edge, wrap);
			}
			if (!target) {
				/*
				 * Most likely because we're already on the
				 * output furthest in the requested direction
				 * or the output or direction was invalid.
				 */
				wlr_log(WLR_DEBUG, "Invalid output");
				break;
			}
			view_move_to_output(view, target);
			break;
		case ACTION_TYPE_FIT_TO_OUTPUT:
			if (!view) {
				break;
			}
			view_constrain_size_to_that_of_usable_area(view);
			break;
		case ACTION_TYPE_SNAP_TO_REGION:
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
				view_snap_to_region(view, region,
					/*store_natural_geometry*/ true);
			} else {
				wlr_log(WLR_ERROR, "Invalid SnapToRegion id: '%s'", region_name);
			}
			break;
		case ACTION_TYPE_TOGGLE_KEYBINDS:
			if (view) {
				view_toggle_keybinds(view);
			}
			break;
		case ACTION_TYPE_FOCUS_OUTPUT:
			{
				const char *output_name = action_get_str(action, "output", NULL);
				desktop_focus_output(output_from_name(server, output_name));
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
						actions_run(view, server, actions, 0);
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
				struct wlr_box geometry = view->pending;
				if (placement_find_best(view, &geometry)) {
					view_move(view, geometry.x, geometry.y);
				}
			}
			break;
		case ACTION_TYPE_TOGGLE_TEARING:
			if (view) {
				view->tearing_hint = !view->tearing_hint;
				wlr_log(WLR_DEBUG, "tearing %sabled",
					view->tearing_hint ? "en" : "dis");
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
