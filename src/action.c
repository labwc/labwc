// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "common/spawn.h"
#include "debug.h"
#include "labwc.h"
#include "menu/menu.h"
#include "private/action.h"
#include "ssd.h"
#include "view.h"
#include "workspaces.h"

enum action_type {
	ACTION_TYPE_INVALID = 0,
	ACTION_TYPE_NONE,
	ACTION_TYPE_CLOSE,
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
	ACTION_TYPE_TOGGLE_FULLSCREEN,
	ACTION_TYPE_TOGGLE_DECORATIONS,
	ACTION_TYPE_TOGGLE_ALWAYS_ON_TOP,
	ACTION_TYPE_FOCUS,
	ACTION_TYPE_ICONIFY,
	ACTION_TYPE_MOVE,
	ACTION_TYPE_RAISE,
	ACTION_TYPE_RESIZE,
	ACTION_TYPE_GO_TO_DESKTOP,
	ACTION_TYPE_SEND_TO_DESKTOP,
};

const char *action_names[] = {
	"INVALID",
	"None",
	"Close",
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
	"ToggleFullscreen",
	"ToggleDecorations",
	"ToggleAlwaysOnTop",
	"Focus",
	"Iconify",
	"Move",
	"Raise",
	"Resize",
	"GoToDesktop",
	"SendToDesktop",
	NULL
};

static char *
action_str_from_arg(struct action_arg *arg)
{
	assert(arg->type == LAB_ACTION_ARG_STR);
	return ((struct action_arg_str *)arg)->value;
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
	struct action *action = znew(*action);
	action->type = action_type_from_str(action_name);
	wl_list_init(&action->args);
	return action;
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
				free(action_str_from_arg(arg));
			}
			zfree(arg);
		}
		zfree(action);
	}
}

static void
show_menu(struct server *server, struct view *view, const char *menu_name)
{
	bool force_menu_top_left = false;
	struct menu *menu = menu_get_by_id(menu_name);
	if (!menu) {
		return;
	}
	if (!strcasecmp(menu_name, "client-menu")) {
		if (!view) {
			return;
		}
		enum ssd_part_type type = ssd_at(view, server->seat.cursor->x,
			server->seat.cursor->y);
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
		x = view->x;
		y = view->y;
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
		wlr_log(WLR_DEBUG, "Handling action %s (%u)",
			 action_names[action->type], action->type);

		/* Get arg now so we don't have to repeat every time we only need one */
		arg = action_get_first_arg(action);

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
				desktop_move_to_front(view);
			}
			break;
		case ACTION_TYPE_RESIZE:
			if (view) {
				interactive_begin(view, LAB_INPUT_STATE_RESIZE,
					resize_edges);
			}
			break;
		case ACTION_TYPE_GO_TO_DESKTOP:
			if (!arg) {
				wlr_log(WLR_ERROR, "Missing argument for GoToDesktop");
				break;
			}
			struct workspace *target;
			char *target_name = action_str_from_arg(arg);
			target = workspaces_find(server->workspace_current, target_name);
			if (target) {
				workspaces_switch_to(target);
			}
			break;
		case ACTION_TYPE_SEND_TO_DESKTOP:
			if (!arg) {
				wlr_log(WLR_ERROR, "Missing argument for SendToDesktop");
				break;
			}
			if (view) {
				struct workspace *target;
				char *target_name = action_str_from_arg(arg);
				target = workspaces_find(view->workspace, target_name);
				if (target) {
					workspaces_send_to(view, target);
				}
			}
			break;
		case ACTION_TYPE_NONE:
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
