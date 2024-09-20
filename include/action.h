/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ACTION_H
#define LABWC_ACTION_H

#include <stdbool.h>
#include <wayland-util.h>

struct view;
struct server;
struct cursor_context;

struct action {
	struct wl_list link; /*
			      * struct keybinding.actions
			      * struct mousebinding.actions
			      * struct menuitem.actions
			      */

	uint32_t type;        /* enum action_type */
	struct wl_list args;  /* struct action_arg.link */
};

struct action *action_create(const char *action_name);

bool action_is_valid(struct action *action);

void action_arg_add_str(struct action *action, const char *key, const char *value);
void action_arg_add_actionlist(struct action *action, const char *key);
void action_arg_add_querylist(struct action *action, const char *key);

struct wl_list *action_get_actionlist(struct action *action, const char *key);
struct wl_list *action_get_querylist(struct action *action, const char *key);

void action_arg_from_xml_node(struct action *action, const char *nodename, const char *content);

bool actions_contain_toggle_keybinds(struct wl_list *action_list);

/**
 * actions_run() - Run actions.
 * @activator: Target view to apply actions (e.g. Maximize, Focus etc.).
 * NULL is allowed, in which case the focused/hovered view is used.
 * @ctx: Set for action invocations via mousebindings. Used to get the
 * direction of resize or the position of the window menu button for ShowMenu
 * action.
 */
void actions_run(struct view *activator, struct server *server,
	struct wl_list *actions, struct cursor_context *ctx);

void action_free(struct action *action);
void action_list_free(struct wl_list *action_list);

#endif /* LABWC_ACTION_H */
