// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "debug.h"
#include "labwc.h"
#include "menu/menu.h"
#include "regions.h"
#include "ssd.h"
#include "view.h"
#include "workspaces.h"

enum action_arg_type {
	LAB_ACTION_ARG_STR = 0,
	LAB_ACTION_ARG_BOOL,
	LAB_ACTION_ARG_INT,
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
	ACTION_TYPE_FOCUS,
	ACTION_TYPE_ICONIFY,
	ACTION_TYPE_MOVE,
	ACTION_TYPE_RAISE,
	ACTION_TYPE_LOWER,
	ACTION_TYPE_RESIZE,
	ACTION_TYPE_MOVETO,
	ACTION_TYPE_GO_TO_DESKTOP,
	ACTION_TYPE_SEND_TO_DESKTOP,
	ACTION_TYPE_SNAP_TO_REGION,
	ACTION_TYPE_TOGGLE_KEYBINDS,
	ACTION_TYPE_FOCUS_OUTPUT,
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
	"Focus",
	"Iconify",
	"Move",
	"Raise",
	"Lower",
	"Resize",
	"MoveTo",
	"GoToDesktop",
	"SendToDesktop",
	"SnapToRegion",
	"ToggleKeybinds",
	"FocusOutput",
	NULL
};

void
action_arg_add_str(struct action *action, char *key, const char *value)
{
	assert(value && "Tried to add NULL action string argument");
	struct action_arg_str *arg = znew(*arg);
	arg->base.type = LAB_ACTION_ARG_STR;
	if (key) {
		arg->base.key = xstrdup(key);
	}
	arg->value = xstrdup(value);
	wl_list_append(&action->args, &arg->base.link);
}

static void
action_arg_add_bool(struct action *action, char *key, bool value)
{
	struct action_arg_bool *arg = znew(*arg);
	arg->base.type = LAB_ACTION_ARG_BOOL;
	if (key) {
		arg->base.key = xstrdup(key);
	}
	arg->value = value;
	wl_list_append(&action->args, &arg->base.link);
}

static void
action_arg_add_int(struct action *action, char *key, int value)
{
	struct action_arg_int *arg = znew(*arg);
	arg->base.type = LAB_ACTION_ARG_INT;
	if (key) {
		arg->base.key = xstrdup(key);
	}
	arg->value = value;
	wl_list_append(&action->args, &arg->base.link);
}

void
action_arg_from_xml_node(struct action *action, char *nodename, char *content)
{
	assert(action);

	char *argument = xstrdup(nodename);
	string_truncate_at_pattern(argument, ".action");

	switch (action->type) {
	case ACTION_TYPE_EXECUTE:
		if (!strcmp(argument, "command")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		} else if (!strcmp(argument, "execute")) {
			/*
			 * <action name="Execute"><execute>foo</execute></action>
			 * is deprecated, but we support it anyway for backward
			 * compatibility with old openbox-menu generators
			 */
			action_arg_add_str(action, "command", content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_MOVE_TO_EDGE:
	case ACTION_TYPE_SNAP_TO_EDGE:
		if (!strcmp(argument, "direction")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_SHOW_MENU:
		if (!strcmp(argument, "menu")) {
			action_arg_add_str(action, argument, content);
			goto cleanup;
		}
		break;
	case ACTION_TYPE_MOVETO:
		if (!strcmp(argument, "x") || !strcmp(argument, "y")) {
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
		};
		break;
	case ACTION_TYPE_FOCUS_OUTPUT:
		if (!strcmp(argument, "output")) {
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

static const char *
action_str_from_arg(struct action_arg *arg)
{
	assert(arg->type == LAB_ACTION_ARG_STR);
	return ((struct action_arg_str *)arg)->value;
}

static const char *
get_arg_value_str(struct action *action, const char *key, const char *default_value)
{
	assert(key);
	struct action_arg *arg;
	wl_list_for_each(arg, &action->args, link) {
		if (!arg->key) {
			continue;
		}
		if (!strcasecmp(key, arg->key)) {
			return action_str_from_arg(arg);
		}
	}
	return default_value;
}

static bool
get_arg_value_bool(struct action *action, const char *key, bool default_value)
{
	assert(key);
	struct action_arg *arg;
	wl_list_for_each(arg, &action->args, link) {
		if (!arg->key) {
			continue;
		}
		if (!strcasecmp(key, arg->key)) {
			assert(arg->type == LAB_ACTION_ARG_BOOL);
			return ((struct action_arg_bool *)arg)->value;
		}
	}
	return default_value;
}

static int
get_arg_value_int(struct action *action, const char *key, int default_value)
{
	assert(key);
	struct action_arg *arg;
	wl_list_for_each(arg, &action->args, link) {
		if (!arg->key) {
			continue;
		}
		if (!strcasecmp(key, arg->key)) {
			assert(arg->type == LAB_ACTION_ARG_INT);
			return ((struct action_arg_int *)arg)->value;
		}
	}
	return default_value;
}

static struct action_arg *
action_get_first_arg(struct action *action)
{
	struct action_arg *arg;
	struct wl_list *item = action->args.next;
	if (item == &action->args) {
		return NULL;
	}
	return wl_container_of(item, arg, link);
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

void action_list_free(struct wl_list *action_list)
{
	struct action_arg *arg, *arg_tmp;
	struct action *action, *action_tmp;
	/* Free actions */
	wl_list_for_each_safe(action, action_tmp, action_list, link) {
		wl_list_remove(&action->link);
		/* Free args */
		wl_list_for_each_safe(arg, arg_tmp, &action->args, link) {
			wl_list_remove(&arg->link);
			zfree(arg->key);
			if (arg->type == LAB_ACTION_ARG_STR) {
				free((void *)action_str_from_arg(arg));
			}
			zfree(arg);
		}
		zfree(action);
	}
}

static void
show_menu(struct server *server, struct view *view, const char *menu_name)
{
	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH
			&& server->input_mode != LAB_INPUT_STATE_MENU) {
		/* Prevent opening a menu while resizing / moving a view */
		return;
	}

	bool force_menu_top_left = false;
	struct menu *menu = menu_get_by_id(menu_name);
	if (!menu) {
		return;
	}
	if (!strcasecmp(menu_name, "client-menu")) {
		if (!view) {
			return;
		}
		enum ssd_part_type type = ssd_at(view->ssd, server->scene,
			server->seat.cursor->x, server->seat.cursor->y);
		if (type == LAB_SSD_BUTTON_WINDOW_MENU) {
			force_menu_top_left = true;
		} else if (ssd_part_contains(LAB_SSD_PART_TITLEBAR, type)) {
			force_menu_top_left = false;
		} else {
			force_menu_top_left = true;
		}
	}

	int x, y;
	if (force_menu_top_left) {
		x = view->current.x;
		y = view->current.y;
	} else {
		x = server->seat.cursor->x;
		y = server->seat.cursor->y;
	}
	/* Replaced by next show_menu() or cleaned on view_destroy() */
	menu->triggered_by_view = view;
	menu_open(menu, x, y);
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
		return desktop_focused_view(server);
	}
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
	struct action_arg *arg;
	wl_list_for_each(action, actions, link) {
		/* Get arg now so we don't have to repeat every time we only need one */
		arg = action_get_first_arg(action);

		if (arg && arg->type == LAB_ACTION_ARG_STR) {
			wlr_log(WLR_DEBUG, "Handling action %u: %s %s",
				action->type, action_names[action->type],
				action_str_from_arg(arg));
		} else {
			wlr_log(WLR_DEBUG, "Handling action %u: %s",
				action->type, action_names[action->type]);
		}

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
			if (!arg) {
				wlr_log(WLR_ERROR, "Missing argument for Execute");
				break;
			}
			struct buf cmd;
			buf_init(&cmd);
			buf_add(&cmd, action_str_from_arg(arg));
			buf_expand_shell_variables(&cmd);
			spawn_async_no_shell(cmd.buf);
			free(cmd.buf);
			break;
		case ACTION_TYPE_EXIT:
			wl_display_terminate(server->wl_display);
			break;
		case ACTION_TYPE_MOVE_TO_EDGE:
			if (!arg) {
				wlr_log(WLR_ERROR, "Missing argument for MoveToEdge");
				break;
			}
			if (view) {
				view_move_to_edge(view, action_str_from_arg(arg));
			}
			break;
		case ACTION_TYPE_SNAP_TO_EDGE:
			if (!arg) {
				wlr_log(WLR_ERROR, "Missing argument for SnapToEdge");
				break;
			}
			if (view) {
				view_snap_to_edge(view, action_str_from_arg(arg),
					/*store_natural_geometry*/ true);
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
			if (arg) {
				show_menu(server, view, action_str_from_arg(arg));
			} else {
				wlr_log(WLR_ERROR, "Missing argument for ShowMenu");
			}
			break;
		case ACTION_TYPE_TOGGLE_MAXIMIZE:
			if (view) {
				view_toggle_maximize(view);
			}
			break;
		case ACTION_TYPE_MAXIMIZE:
			if (view) {
				view_maximize(view, true, /*store_natural_geometry*/ true);
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
		case ACTION_TYPE_FOCUS:
			if (view) {
				desktop_focus_and_activate_view(&server->seat, view);
			}
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
		case ACTION_TYPE_MOVETO:
			if (view) {
				int x = get_arg_value_int(action, "x", 0);
				int y = get_arg_value_int(action, "y", 0);
				view_move(view, x, y);
			}
			break;
		case ACTION_TYPE_GO_TO_DESKTOP: {
			const char *to = get_arg_value_str(action, "to", NULL);
			if (!to) {
				wlr_log(WLR_ERROR,
					"Missing 'to' argument for GoToDesktop");
				break;
			}
			bool wrap = get_arg_value_bool(action, "wrap", true);
			struct workspace *target;
			target = workspaces_find(server->workspace_current, to, wrap);
			if (target) {
				workspaces_switch_to(target);
			}
			break;
		}
		case ACTION_TYPE_SEND_TO_DESKTOP:
			if (view) {
				const char *to = get_arg_value_str(action, "to", NULL);
				if (!to) {
					wlr_log(WLR_ERROR,
						"Missing 'to' argument for SendToDesktop");
					break;
				}
				bool follow = get_arg_value_bool(action, "follow", true);
				bool wrap = get_arg_value_bool(action, "wrap", true);
				struct workspace *target;
				target = workspaces_find(view->workspace, to, wrap);
				if (target) {
					view_move_to_workspace(view, target);
					if (follow) {
						workspaces_switch_to(target);
					}
				}
			}
			break;
		case ACTION_TYPE_SNAP_TO_REGION:
			if (!arg) {
				wlr_log(WLR_ERROR, "Missing argument for SnapToRegion");
				break;
			}
			if (!view) {
				break;
			}
			struct output *output = view->output;
			if (!output) {
				break;
			}
			const char *region_name = action_str_from_arg(arg);
			struct region *region = regions_from_name(region_name, output);
			if (region) {
				view_snap_to_region(view, region,
					/*store_natural_geometry*/ true);
			} else {
				wlr_log(WLR_ERROR, "Invalid SnapToRegion id: '%s'", region_name);
			}
			break;
		case ACTION_TYPE_TOGGLE_KEYBINDS:
			server->seat.inhibit_keybinds = !server->seat.inhibit_keybinds;
			wlr_log(WLR_DEBUG, "%s keybinds",
				server->seat.inhibit_keybinds ? "Disabled" : "Enabled");
			break;
		case ACTION_TYPE_FOCUS_OUTPUT:
			if (!arg) {
				wlr_log(WLR_ERROR, "Missing argument for FocusOutput");
				break;
			}
			const char *output_name = action_str_from_arg(arg);
			desktop_focus_output(output_from_name(server, output_name));
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

