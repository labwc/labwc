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

/* state-machine variables for processing <item></item> */
static bool in_item;
static struct menuitem *current_item;

#define MENUWIDTH (110)
#define MENU_ITEM_PADDING_Y (4)
#define MENU_ITEM_PADDING_X (7)

static struct menuitem *
menuitem_create(struct server *server, struct menu *menu, const char *text)
{
	struct menuitem *menuitem = calloc(1, sizeof(struct menuitem));
	if (!menuitem) {
		return NULL;
	}
	struct theme *theme = server->theme;
	struct font font = {
		.name = rc.font_name_menuitem,
		.size = rc.font_size_menuitem,
	};

	menuitem->box.width = MENUWIDTH;
	menuitem->box.height = font_height(&font) + 2 * MENU_ITEM_PADDING_Y;

	int item_max_width = MENUWIDTH - 2 * MENU_ITEM_PADDING_X;
	font_texture_create(server, &menuitem->texture.active, item_max_width,
		text, &font, theme->menu_items_active_text_color);
	font_texture_create(server, &menuitem->texture.inactive, item_max_width,
		text, &font, theme->menu_items_text_color);

	/* center align vertically */
	menuitem->texture.offset_y =
		(menuitem->box.height - menuitem->texture.active->height) / 2;
	menuitem->texture.offset_x = MENU_ITEM_PADDING_X;

	wl_list_insert(&menu->menuitems, &menuitem->link);
	return menuitem;
}

static void fill_item(char *nodename, char *content, struct menu *menu)
{
	string_truncate_at_pattern(nodename, ".item.menu");

	if (getenv("LABWC_DEBUG_MENU_NODENAMES")) {
		printf("%s: %s\n", nodename, content);
	}

	/*
	 * Handle the following:
	 * <item label="">
	 *   <action name="">
	 *     <command></command>
	 *   </action>
	 * </item>
	 */
	if (!strcmp(nodename, "label")) {
		current_item = menuitem_create(menu->server, menu, content);
	}
	assert(current_item);
	if (!strcmp(nodename, "name.action")) {
		current_item->action = strdup(content);
	} else if (!strcmp(nodename, "command.action")) {
		current_item->command = strdup(content);
	}
}

static void
entry(xmlNode *node, char *nodename, char *content, struct menu *menu)
{
	static bool in_root_menu;

	if (!nodename) {
		return;
	}
	string_truncate_at_pattern(nodename, ".openbox_menu");
	if (!content) {
		return;
	}
	if (!strcmp(nodename, "id.menu")) {
		in_root_menu = !strcmp(content, "root-menu") ? true : false;
	}

	/* We only handle the root-menu for the time being */
	if (!in_root_menu) {
		return;
	}
	if (in_item) {
		fill_item(nodename, content, menu);
	}
}

static void
process_node(xmlNode *node, struct menu *menu)
{
	char *content;
	static char buffer[256];
	char *name;

	content = (char *)node->content;
	if (xmlIsBlankNode(node)) {
		return;
	}
	name = nodename(node, buffer, sizeof(buffer));
	entry(node, name, content, menu);
}

static void xml_tree_walk(xmlNode *node, struct menu *menu);

static void
traverse(xmlNode *n, struct menu *menu)
{
	xmlAttr *attr;

	process_node(n, menu);
	for (attr = n->properties; attr; attr = attr->next) {
		xml_tree_walk(attr->children, menu);
	}
	xml_tree_walk(n->children, menu);
}

static void
xml_tree_walk(xmlNode *node, struct menu *menu)
{
	for (xmlNode *n = node; n && n->name; n = n->next) {
		if (!strcasecmp((char *)n->name, "comment")) {
			continue;
		}
		if (!strcasecmp((char *)n->name, "item")) {
			in_item = true;
			traverse(n, menu);
			in_item = false;
			continue;
		}
		traverse(n, menu);
	}
}

static void
parse_xml(const char *filename, struct menu *menu)
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
		if (p)
			*p = '\0';
		buf_add(&b, line);
	}
	free(line);
	fclose(stream);
	xmlDoc *d = xmlParseMemory(b.buf, b.len);
	if (!d) {
		wlr_log(WLR_ERROR, "xmlParseMemory()");
		exit(EXIT_FAILURE);
	}
	xml_tree_walk(xmlDocGetRootElement(d), menu);
	xmlFreeDoc(d);
	xmlCleanupParser();
	free(b.buf);
}

void
menu_init_rootmenu(struct server *server, struct menu *menu)
{
	static bool has_run;

	if (!has_run) {
		LIBXML_TEST_VERSION
		wl_list_init(&menu->menuitems);
		server->rootmenu = menu;
		menu->server = server;
	}

	parse_xml("menu.xml", menu);

	/* Default menu if no menu.xml found */
	if (wl_list_empty(&menu->menuitems)) {
		current_item = menuitem_create(server, menu, "Reconfigure");
		current_item->action = strdup("Reconfigure");
		current_item = menuitem_create(server, menu, "Exit");
		current_item->action = strdup("Exit");
	}
	menu_move(menu, 100, 100);
}

void
menu_finish(struct menu *menu)
{
	struct menuitem *menuitem, *next;
	wl_list_for_each_safe(menuitem, next, &menu->menuitems, link) {
		zfree(menuitem->action);
		zfree(menuitem->command);
		wl_list_remove(&menuitem->link);
		free(menuitem);
	}
}

/*
 * TODO: call this menu_configure and return the size of the menu so that
 * a background color can be rendered
 */
void
menu_move(struct menu *menu, int x, int y)
{
	menu->box.x = x;
	menu->box.y = y;

	int offset = 0;
	struct menuitem *menuitem;
	wl_list_for_each_reverse (menuitem, &menu->menuitems, link) {
		menuitem->box.x = menu->box.x;
		menuitem->box.y = menu->box.y + offset;
		offset += menuitem->box.height;
	}

	menu->box.width = MENUWIDTH;
	menu->box.height = offset;
}

void
menu_set_selected(struct menu *menu, int x, int y)
{
	struct menuitem *menuitem;
	wl_list_for_each (menuitem, &menu->menuitems, link) {
		menuitem->selected =
			wlr_box_contains_point(&menuitem->box, x, y);
	}
}

void
menu_action_selected(struct server *server, struct menu *menu)
{
	struct menuitem *menuitem;
	wl_list_for_each (menuitem, &menu->menuitems, link) {
		if (menuitem->selected) {
			action(server, menuitem->action, menuitem->command);
			break;
		}
	}
}

void
menu_reconfigure(struct server *server, struct menu *menu)
{
	menu_finish(menu);
	menu_init_rootmenu(server, menu);
}
