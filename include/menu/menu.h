/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_MENU_H
#define __LABWC_MENU_H

#include <wayland-server.h>

/* forward declare arguments */
struct view;
struct server;
struct wl_list;
struct wlr_scene_tree;
struct wlr_scene_node;
struct scaled_font_buffer;

enum menu_align {
	LAB_MENU_OPEN_AUTO   = 0,
	LAB_MENU_OPEN_LEFT   = 1 << 0,
	LAB_MENU_OPEN_RIGHT  = 1 << 1,
	LAB_MENU_OPEN_TOP    = 1 << 2,
	LAB_MENU_OPEN_BOTTOM = 1 << 3,
};

struct menu_scene {
	struct wlr_scene_tree *tree;
	struct wlr_scene_node *text;
	struct wlr_scene_node *background;
	struct scaled_font_buffer *buffer;
};

struct menuitem {
	struct wl_list actions;
	struct menu *parent;
	struct menu *submenu;
	bool selectable;
	int height;
	int native_width;
	struct wlr_scene_tree *tree;
	struct menu_scene normal;
	struct menu_scene selected;
	struct wl_list link; /* menu::menuitems */
};

/* This could be the root-menu or a submenu */
struct menu {
	char *id;
	char *label;
	int item_height;
	struct menu *parent;
	struct {
		int width;
		int height;
	} size;
	struct wl_list menuitems;
	struct server *server;
	struct {
		struct menu *menu;
		struct menuitem *item;
	} selection;
	struct wlr_scene_tree *scene_tree;

	/* Used to match a window-menu to the view that triggered it. */
	struct view *triggered_by_view;  /* may be NULL */
};

void menu_init(struct server *server);
void menu_finish(void);

/**
 * menu_get_by_id - get menu by id
 *
 * @id id string defined in menu.xml like "root-menu"
 */
struct menu *menu_get_by_id(const char *id);

/**
 * menu_open - open menu on position (x, y)
 *
 * This function will close server->menu_current, open the
 * new menu and assign @menu to server->menu_current.
 *
 * Additionally, server->input_mode wil be set to LAB_INPUT_STATE_MENU.
 */
void menu_open(struct menu *menu, int x, int y);

/**
 * menu_process_cursor_motion
 *
 * - handles hover effects
 * - may open/close submenus
 */
void menu_process_cursor_motion(struct wlr_scene_node *node);

/**
 * menu_call_actions - call actions associated with a menu node
 *
 * If menuitem connected to @node does not just open a submenu:
 * - associated actions will be called
 * - server->menu_current will be closed
 * - server->menu_current will be set to NULL
 *
 * Returns true if actions have actually been executed
 */
bool menu_call_actions(struct wlr_scene_node *node);

/**
 *  menu_close_root- close root menu
 *
 * This function will close server->menu_current and set it to NULL.
 * Asserts that server->input_mode is set to LAB_INPUT_STATE_MENU.
 *
 * Additionally, server->input_mode wil be set to LAB_INPUT_STATE_PASSTHROUGH.
 */
void menu_close_root(struct server *server);

/* menu_reconfigure - reload theme and content */
void menu_reconfigure(struct server *server);

#endif /* __LABWC_MENU_H */
