#ifndef __LABWC_MENU_H
#define __LABWC_MENU_H

#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>

struct menuitem {
	char *action;
	char *command;
	struct wlr_box geo_box;
	struct wlr_texture *active_texture;
	struct wlr_texture *inactive_texture;
	bool selected;
	struct wl_list link;
};

struct menu {
	int x;
	int y;
	struct wl_list menuitems;
};

/* menu_create - create menu */
void menu_init(struct server *server, struct menu *menu);
void menu_finish(struct menu *menu);

/* menu_move - move to position (x, y) */
void menu_move(struct menu *menu, int x, int y);

/* menu_set_selected - select item at (x, y) */
void menu_set_selected(struct menu *menu, int x, int y);

/* menu_action_selected - select item at (x, y) */
void menu_action_selected(struct server *server, struct menu *menu);

#endif /* __LABWC_MENU_H */
