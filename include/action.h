/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ACTION_H
#define LABWC_ACTION_H

#include <wayland-util.h>

struct view;
struct server;

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

void action_arg_from_xml_node(struct action *action, const char *nodename, const char *content);

bool actions_contain_toggle_keybinds(struct wl_list *action_list);

void actions_run(struct view *activator, struct server *server,
	struct wl_list *actions, uint32_t resize_edges);

void action_free(struct action *action);
void action_list_free(struct wl_list *action_list);

#endif /* LABWC_ACTION_H */
