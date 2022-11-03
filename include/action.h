/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_ACTION_H
#define __LABWC_ACTION_H

struct view;
struct server;
struct wl_list;

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
void action_arg_add_str(struct action *action, char *key, const char *value);
void actions_run(struct view *activator, struct server *server,
	struct wl_list *actions, uint32_t resize_edges);
void action_list_free(struct wl_list *action_list);

#endif /* __LABWC_ACTION_H */
