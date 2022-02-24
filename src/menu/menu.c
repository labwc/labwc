// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/dir.h"
#include "common/font.h"
#include "common/nodename.h"
#include "common/string-helpers.h"
#include "common/zfree.h"
#include "labwc.h"
#include "menu/menu.h"
#include "theme.h"
#include "action.h"
#include "buffer.h"

#define MENUWIDTH (110)
#define MENU_ITEM_PADDING_Y (4)
#define MENU_ITEM_PADDING_X (7)

/* state-machine variables for processing <item></item> */
static bool in_item;
static struct menuitem *current_item;
static struct action *current_item_action;

static int menu_level;
static struct menu *current_menu;

/* vector for <menu id="" label=""> elements */
static struct menu *menus;
static int nr_menus, alloc_menus;

static struct menu *
menu_create(struct server *server, const char *id, const char *label)
{
	if (nr_menus == alloc_menus) {
		alloc_menus = (alloc_menus + 16) * 2;
		menus = realloc(menus, alloc_menus * sizeof(struct menu));
	}
	struct menu *menu = menus + nr_menus;
	memset(menu, 0, sizeof(*menu));
	nr_menus++;
	wl_list_init(&menu->menuitems);
	menu->id = strdup(id);
	menu->label = strdup(label);
	menu->parent = current_menu;
	menu->server = server;
	menu->size.width = MENUWIDTH;
	/* menu->size.height will be kept up to date by adding items */
	menu->scene_tree = wlr_scene_tree_create(&server->menu_tree->node);
	wlr_scene_node_set_enabled(&menu->scene_tree->node, false);
	return menu;
}

struct menu *
menu_get_by_id(const char *id)
{
	if (!id) {
		return NULL;
	}
	struct menu *menu;
	for (int i = 0; i < nr_menus; ++i) {
		menu = menus + i;
		if (!strcmp(menu->id, id)) {
			return menu;
		}
	}
	return NULL;
}

static struct menuitem *
item_create(struct menu *menu, const char *text)
{
	struct menuitem *menuitem = calloc(1, sizeof(struct menuitem));
	if (!menuitem) {
		return NULL;
	}
	struct server *server = menu->server;
	struct theme *theme = server->theme;
	struct font font = {
		.name = rc.font_name_menuitem,
		.size = rc.font_size_menuitem,
	};

	if (!menu->item_height) {
		menu->item_height = font_height(&font) + 2 * MENU_ITEM_PADDING_Y;
	}

	int x, y;
	int item_max_width = MENUWIDTH - 2 * MENU_ITEM_PADDING_X;
	struct wlr_scene_node *parent = &menu->scene_tree->node;

	/* Font buffer */
	font_buffer_create(&menuitem->normal.buffer, item_max_width,
		text, &font, theme->menu_items_text_color);
	font_buffer_create(&menuitem->selected.buffer, item_max_width,
		text, &font, theme->menu_items_active_text_color);

	/* Item background nodes */
	menuitem->normal.background = &wlr_scene_rect_create(parent,
		MENUWIDTH, menu->item_height,
		theme->menu_items_bg_color)->node;
	menuitem->selected.background = &wlr_scene_rect_create(parent,
		MENUWIDTH, menu->item_height,
		theme->menu_items_active_bg_color)->node;

	/* Font nodes */
	menuitem->normal.text = &wlr_scene_buffer_create(
		menuitem->normal.background, &menuitem->normal.buffer->base)->node;
	menuitem->selected.text = &wlr_scene_buffer_create(
		menuitem->selected.background, &menuitem->selected.buffer->base)->node;

	/* Center font nodes */
	y = (menu->item_height - menuitem->normal.buffer->base.height) / 2;
	x = MENU_ITEM_PADDING_X;
	wlr_scene_node_set_position(menuitem->normal.text, x, y);
	wlr_scene_node_set_position(menuitem->selected.text, x, y);

	/* Position the item in relation to its menu */
	int item_count = wl_list_length(&menu->menuitems);
	y = item_count * menu->item_height;
	wlr_scene_node_set_position(menuitem->normal.background, 0, y);
	wlr_scene_node_set_position(menuitem->selected.background, 0, y);

	/* Hide selected state */
	wlr_scene_node_set_enabled(menuitem->selected.background, false);

	/* Update menu extends */
	menu->size.height = (item_count + 1) * menu->item_height;

	wl_list_insert(&menu->menuitems, &menuitem->link);
	wl_list_init(&menuitem->actions);
	return menuitem;
}

/*
 * Handle the following:
 * <item label="">
 *   <action name="">
 *     <command></command>
 *   </action>
 * </item>
 */
static void
fill_item(char *nodename, char *content)
{
	string_truncate_at_pattern(nodename, ".item.menu");

	/* <item label=""> defines the start of a new item */
	if (!strcmp(nodename, "label")) {
		current_item = item_create(current_menu, content);
		current_item_action = NULL;
	} else if (!current_item) {
		wlr_log(WLR_ERROR, "expect <item label=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcmp(nodename, "name.action")) {
		current_item_action = action_create(content);
		wl_list_insert(current_item->actions.prev, &current_item_action->link);
	} else if (!current_item_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcmp(nodename, "command.action")) {
		current_item_action->arg = strdup(content);
	}
}

static void
entry(xmlNode *node, char *nodename, char *content)
{
	if (!nodename || !content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".openbox_menu");
	if (in_item) {
		fill_item(nodename, content);
	}
}

static void
process_node(xmlNode *node)
{
	static char buffer[256];

	char *content = (char *)node->content;
	if (xmlIsBlankNode(node)) {
		return;
	}
	char *name = nodename(node, buffer, sizeof(buffer));
	entry(node, name, content);
}

static void xml_tree_walk(xmlNode *node, struct server *server);

static void
traverse(xmlNode *n, struct server *server)
{
	xmlAttr *attr;

	process_node(n);
	for (attr = n->properties; attr; attr = attr->next) {
		xml_tree_walk(attr->children, server);
	}
	xml_tree_walk(n->children, server);
}

/*
 * <menu> elements have three different roles:
 *  * Definition of (sub)menu - has ID, LABEL and CONTENT
 *  * Menuitem of pipemenu type - has EXECUTE and LABEL
 *  * Menuitem of submenu type - has ID only
 */
static void
handle_menu_element(xmlNode *n, struct server *server)
{
	char *label = (char *)xmlGetProp(n, (const xmlChar *)"label");
	char *execute = (char *)xmlGetProp(n, (const xmlChar *)"execute");
	char *id = (char *)xmlGetProp(n, (const xmlChar *)"id");

	if (execute) {
		wlr_log(WLR_ERROR, "we do not support pipemenus");
	} else if (label && id) {
		struct menu **submenu = NULL;
		if (menu_level > 0) {
			/*
			 * In a nested (inline) menu definition we need to
			 * create an item pointing to the new submenu
			 */
			current_item = item_create(current_menu, label);
			submenu = &current_item->submenu;
		}
		++menu_level;
		current_menu = menu_create(server, id, label);
		if (submenu) {
			*submenu = current_menu;
		}
		traverse(n, server);
		current_menu = current_menu->parent;
		--menu_level;
	} else if (id) {
		struct menu *menu = menu_get_by_id(id);
		if (menu) {
			current_item = item_create(current_menu, menu->label);
			current_item->submenu = menu;
		} else {
			wlr_log(WLR_ERROR, "no menu with id '%s'", id);
		}
	}
	zfree(label);
	zfree(execute);
	zfree(id);
}

static void
xml_tree_walk(xmlNode *node, struct server *server)
{
	for (xmlNode *n = node; n && n->name; n = n->next) {
		if (!strcasecmp((char *)n->name, "comment")) {
			continue;
		}
		if (!strcasecmp((char *)n->name, "menu")) {
			handle_menu_element(n, server);
			continue;
		}
		if (!strcasecmp((char *)n->name, "item")) {
			in_item = true;
			traverse(n, server);
			in_item = false;
			continue;
		}
		traverse(n, server);
	}
}

static void
parse_xml(const char *filename, struct server *server)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	struct buf b;
	static char menuxml[4096] = { 0 };

	if (!strlen(config_dir())) {
		return;
	}
	snprintf(menuxml, sizeof(menuxml), "%s/%s", config_dir(), filename);

	stream = fopen(menuxml, "r");
	if (!stream) {
		wlr_log(WLR_ERROR, "cannot read %s", menuxml);
		return;
	}
	wlr_log(WLR_INFO, "read menu file %s", menuxml);
	buf_init(&b);
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		buf_add(&b, line);
	}
	free(line);
	fclose(stream);
	xmlDoc *d = xmlParseMemory(b.buf, b.len);
	if (!d) {
		wlr_log(WLR_ERROR, "xmlParseMemory()");
		goto err;
	}
	xml_tree_walk(xmlDocGetRootElement(d), server);
	xmlFreeDoc(d);
	xmlCleanupParser();
err:
	free(b.buf);
}

static int
menu_get_full_width(struct menu *menu)
{
	int width = menu->size.width - menu->server->theme->menu_overlap_x;
	int child_width;
	int max_child_width = 0;
	struct menuitem *item;
	wl_list_for_each_reverse(item, &menu->menuitems, link) {
		if (!item->submenu) {
			continue;
		}
		child_width = menu_get_full_width(item->submenu);
		if (child_width > max_child_width) {
			max_child_width = child_width;
		}
	}
	return width + max_child_width;
}

static void
menu_configure(struct menu *menu, int lx, int ly, enum menu_align align)
{
	struct theme *theme = menu->server->theme;

	/* Get output local coordinates + output usable area */
	double ox = lx;
	double oy = ly;
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		menu->server->output_layout, lx, ly);
	wlr_output_layout_output_coords(menu->server->output_layout,
		wlr_output, &ox, &oy);
	struct wlr_box usable = output_usable_area_from_cursor_coords(menu->server);

	if (align == LAB_MENU_OPEN_AUTO) {
		int full_width = menu_get_full_width(menu);
		if (ox + full_width > usable.width) {
			align = LAB_MENU_OPEN_LEFT;
		} else {
			align = LAB_MENU_OPEN_RIGHT;
		}
	}

	if (oy + menu->size.height > usable.height) {
		align &= ~LAB_MENU_OPEN_BOTTOM;
		align |= LAB_MENU_OPEN_TOP;
	} else {
		align &= ~LAB_MENU_OPEN_TOP;
		align |= LAB_MENU_OPEN_BOTTOM;
	}

	if (align & LAB_MENU_OPEN_LEFT) {
		lx -= MENUWIDTH - theme->menu_overlap_x;
	}
	if (align & LAB_MENU_OPEN_TOP) {
		ly -= menu->size.height;
		if (menu->parent) {
			/* For submenus adjust y to bottom left corner */
			ly += menu->item_height;
		}
	}
	wlr_scene_node_set_position(&menu->scene_tree->node, lx, ly);

	int rel_y;
	int new_lx, new_ly;
	struct menuitem *item;
	wl_list_for_each_reverse(item, &menu->menuitems, link) {
		if (!item->submenu) {
			continue;
		}
		if (align & LAB_MENU_OPEN_RIGHT) {
			new_lx = lx + MENUWIDTH - theme->menu_overlap_x;
		} else {
			new_lx = lx;
		}
		rel_y = item->normal.background->state.y;
		new_ly = ly + rel_y - theme->menu_overlap_y;
		menu_configure(item->submenu, new_lx, new_ly, align);
	}
}

void
menu_init_rootmenu(struct server *server)
{
	parse_xml("menu.xml", server);
	struct menu *menu = menu_get_by_id("root-menu");

	/* Default menu if no menu.xml found */
	if (!menu) {
		current_menu = NULL;
		menu = menu_create(server, "root-menu", "");
	}
	if (wl_list_empty(&menu->menuitems)) {
		current_item = item_create(menu, "Reconfigure");
		fill_item("name.action", "Reconfigure");
		current_item = item_create(menu, "Exit");
		fill_item("name.action", "Exit");
	}
}

void
menu_init_windowmenu(struct server *server)
{
	struct menu *menu = menu_get_by_id("client-menu");

	/* Default menu if no menu.xml found */
	if (!menu) {
		current_menu = NULL;
		menu = menu_create(server, "client-menu", "");
	}
	if (wl_list_empty(&menu->menuitems)) {
		current_item = item_create(menu, "Minimize");
		fill_item("name.action", "Iconify");
		current_item = item_create(menu, "Maximize");
		fill_item("name.action", "ToggleMaximize");
		current_item = item_create(menu, "Fullscreen");
		fill_item("name.action", "ToggleFullscreen");
		current_item = item_create(menu, "Decorations");
		fill_item("name.action", "ToggleDecorations");
		current_item = item_create(menu, "Close");
		fill_item("name.action", "Close");
	}
}

void
menu_finish(void)
{
	struct menu *menu;
	for (int i = 0; i < nr_menus; ++i) {
		menu = menus + i;
		struct menuitem *item, *next;
		wl_list_for_each_safe(item, next, &menu->menuitems, link) {
			wl_list_remove(&item->link);
			action_list_free(&item->actions);
			wlr_scene_node_destroy(item->normal.text);
			wlr_scene_node_destroy(item->selected.text);
			wlr_scene_node_destroy(item->normal.background);
			wlr_scene_node_destroy(item->selected.background);
			wlr_buffer_drop(&item->normal.buffer->base);
			wlr_buffer_drop(&item->selected.buffer->base);
			free(item);
		}
		wlr_scene_node_destroy(&menu->scene_tree->node);
	}
	zfree(menus);
	alloc_menus = 0;
	nr_menus = 0;
}

/* Sets selection (or clears selection if passing NULL) */
static void
menu_set_selection(struct menu *menu, struct menuitem *item)
{
	/* Clear old selection */
	if (menu->selection.item) {
		wlr_scene_node_set_enabled(
			menu->selection.item->normal.background, true);
		wlr_scene_node_set_enabled(
			menu->selection.item->selected.background, false);
	}
	/* Set new selection */
	if (item) {
		wlr_scene_node_set_enabled(item->normal.background, false);
		wlr_scene_node_set_enabled(item->selected.background, true);
	}
	menu->selection.item = item;
}

static void
close_all_submenus(struct menu *menu)
{
	struct menuitem *item;
	wl_list_for_each (item, &menu->menuitems, link) {
		if (item->submenu) {
			wlr_scene_node_set_enabled(&item->submenu->scene_tree->node, false);
			close_all_submenus(item->submenu);
		}
	}
	menu->selection.menu = NULL;
}

void
menu_open(struct menu *menu, int x, int y)
{
	assert(menu);
	if (menu->server->menu_current) {
		menu_close(menu->server->menu_current);
	}
	close_all_submenus(menu);
	menu_set_selection(menu, NULL);
	menu_configure(menu, x, y, LAB_MENU_OPEN_AUTO);
	wlr_scene_node_set_enabled(&menu->scene_tree->node, true);
	menu->server->menu_current = menu;
	menu->server->input_mode = LAB_INPUT_STATE_MENU;
}

void
menu_process_cursor_motion(struct menu *menu, struct wlr_scene_node *node)
{
	if (!node) {
		wlr_log(WLR_ERROR, "menu_process_cursor_motion() node == NULL");
		return;
	}
	assert(menu);

	/* TODO: this would be much easier if we could use node->data */

	struct menuitem *item;
	wl_list_for_each (item, &menu->menuitems, link) {
		if (node == item->selected.background
				|| node->parent == item->selected.background) {
			/* We are on an already selected item */
			return;
		}
		if (node == item->normal.background
				|| node->parent == item->normal.background) {
			/* We are on an item that has new mouse-focus */
			menu_set_selection(menu, item);
			if (menu->selection.menu) {
				/* Close old submenu tree */
				menu_close(menu->selection.menu);
			}
			if (item->submenu) {
				/* And open the new one */
				wlr_scene_node_set_enabled(
					&item->submenu->scene_tree->node, true);
			}
			menu->selection.menu = item->submenu;
			return;
		}
		if (item->submenu && item->submenu == menu->selection.menu) {
			menu_process_cursor_motion(item->submenu, node);
		}
	}
}


bool
menu_call_actions(struct menu *menu, struct wlr_scene_node *node)
{
	/* TODO: this would be much easier if we could use node->data */

	if (!menu->selection.item) {
		/* No item selected in current menu */
		wlr_log(WLR_ERROR, "No item on menu_action_selected");
		return false;
	}
	struct wlr_scene_node *menu_node =
		menu->selection.item->selected.background;
	if (node == menu_node || node->parent == menu_node) {
		/* We found the correct menu item */
		if (menu->selection.item->submenu) {
			/* ..but it just opens a submenu */
			return false;
		}
		actions_run(NULL, menu->server, &menu->selection.item->actions, 0);
		menu_close(menu->server->menu_current);
		menu->server->menu_current = NULL;
		return true;
	}
	if (menu->selection.menu) {
		return menu_call_actions(menu->selection.menu, node);
	}
	wlr_log(WLR_ERROR, "No match on menu_action_selected");
	return false;
}

void
menu_close(struct menu *menu)
{
	if (!menu) {
		wlr_log(WLR_ERROR, "Trying to close non exiting menu");
		return;
	}
	/* TODO: Maybe reset input state here instead of in cursor.c ? */
	wlr_scene_node_set_enabled(&menu->scene_tree->node, false);
	menu_set_selection(menu, NULL);
	if (menu->selection.menu) {
		menu_close(menu->selection.menu);
		menu->selection.menu = NULL;
	}
}

void
menu_reconfigure(struct server *server)
{
	menu_finish();
	menu_init_rootmenu(server);
	menu_init_windowmenu(server);
}
