// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/buf.h"
#include "common/dir.h"
#include "common/font.h"
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/nodename.h"
#include "common/scaled-font-buffer.h"
#include "common/scaled-icon-buffer.h"
#include "common/scaled-rect-buffer.h"
#include "common/scene-helpers.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "labwc.h"
#include "menu/menu.h"
#include "workspaces.h"
#include "view.h"
#include "node.h"
#include "theme.h"

#define PIPEMENU_MAX_BUF_SIZE 1048576  /* 1 MiB */
#define PIPEMENU_TIMEOUT_IN_MS 4000    /* 4 seconds */

#define ICON_SIZE (rc.theme->menu_item_height - 2 * rc.theme->menu_items_padding_y)

/* state-machine variables for processing <item></item> */
static bool in_item;
static struct menuitem *current_item;
static struct action *current_item_action;

static struct menu *current_menu;

static bool waiting_for_pipe_menu;
static struct menuitem *selected_item;

struct menu_pipe_context {
	struct wlr_box anchor_rect;
	struct menu *pipemenu;
	struct buf buf;
	struct wl_event_source *event_read;
	struct wl_event_source *event_timeout;
	pid_t pid;
	int pipe_fd;
};

/* TODO: split this whole file into parser.c and actions.c*/

static bool
is_unique_id(struct server *server, const char *id)
{
	struct menu *menu;
	wl_list_for_each(menu, &server->menus, link) {
		if (!strcmp(menu->id, id)) {
			return false;
		}
	}
	return true;
}

static struct menu *
menu_create(struct server *server, const char *id, const char *label)
{
	if (!is_unique_id(server, id)) {
		wlr_log(WLR_ERROR, "menu id %s already exists", id);
	}

	struct menu *menu = znew(*menu);
	wl_list_append(&server->menus, &menu->link);

	wl_list_init(&menu->menuitems);
	menu->id = xstrdup(id);
	menu->label = xstrdup(label ? label : id);
	menu->parent = current_menu;
	menu->server = server;
	menu->is_pipemenu_child = waiting_for_pipe_menu;
	return menu;
}

struct menu *
menu_get_by_id(struct server *server, const char *id)
{
	if (!id) {
		return NULL;
	}
	struct menu *menu;
	wl_list_for_each(menu, &server->menus, link) {
		if (!strcmp(menu->id, id)) {
			return menu;
		}
	}
	return NULL;
}

static void
validate_menu(struct menu *menu)
{
	struct menuitem *item;
	struct action *action, *action_tmp;
	wl_list_for_each(item, &menu->menuitems, link) {
		wl_list_for_each_safe(action, action_tmp, &item->actions, link) {
			bool is_show_menu = action_is_show_menu(action);
			if (!action_is_valid(action) || is_show_menu) {
				if (is_show_menu) {
					wlr_log(WLR_ERROR, "'ShowMenu' action is"
						" not allowed in menu items");
				}
				wl_list_remove(&action->link);
				action_free(action);
				wlr_log(WLR_ERROR, "Removed invalid menu action");
			}
		}
	}
}

static void
validate(struct server *server)
{
	struct menu *menu;
	wl_list_for_each(menu, &server->menus, link) {
		validate_menu(menu);
	}
}

static struct menuitem *
item_create(struct menu *menu, const char *text, bool show_arrow)
{
	assert(menu);
	assert(text);

	struct menuitem *menuitem = znew(*menuitem);
	menuitem->parent = menu;
	menuitem->selectable = true;
	menuitem->type = LAB_MENU_ITEM;
	menuitem->text = xstrdup(text);
	menuitem->arrow = show_arrow ? "›" : NULL;

	menuitem->native_width = font_width(&rc.font_menuitem, text);
	if (menuitem->arrow) {
		menuitem->native_width += font_width(&rc.font_menuitem, menuitem->arrow);
	}

	wl_list_append(&menu->menuitems, &menuitem->link);
	wl_list_init(&menuitem->actions);
	return menuitem;
}

static struct wlr_scene_tree *
item_create_scene_for_state(struct menuitem *item, float *text_color,
	float *bg_color)
{
	struct menu *menu = item->parent;
	struct theme *theme = menu->server->theme;

	/* Tree to hold background and label buffers */
	struct wlr_scene_tree *tree = wlr_scene_tree_create(item->tree);

	int icon_width = 0;
	int icon_size = ICON_SIZE;
	if (item->parent->has_icons) {
		icon_width = theme->menu_items_padding_x + icon_size;
	}

	int bg_width = menu->size.width - 2 * theme->menu_border_width;
	int arrow_width = item->arrow ?
		font_width(&rc.font_menuitem, item->arrow) : 0;
	int label_max_width = bg_width - 2 * theme->menu_items_padding_x
		- arrow_width - icon_width;

	if (label_max_width <= 0) {
		wlr_log(WLR_ERROR, "not enough space for menu contents");
		return tree;
	}

	/* Create background */
	wlr_scene_rect_create(tree, bg_width, theme->menu_item_height, bg_color);

	/* Create icon */
	bool show_app_icon = !strcmp(item->parent->id, "client-list-combined-menu")
				&& item->client_list_view;
	if (item->icon_name || show_app_icon) {
		struct scaled_icon_buffer *icon_buffer = scaled_icon_buffer_create(
			tree, menu->server, icon_size, icon_size);
		if (item->icon_name) {
			/* icon set via <menu icon="..."> */
			scaled_icon_buffer_set_icon_name(icon_buffer, item->icon_name);
		} else if (show_app_icon) {
			/* app icon in client-list-combined-menu */
			const char *app_id = view_get_string_prop(
				item->client_list_view, "app_id");
			scaled_icon_buffer_set_app_id(icon_buffer, app_id);
		}
		wlr_scene_node_set_position(&icon_buffer->scene_buffer->node,
			theme->menu_items_padding_x, theme->menu_items_padding_y);
	}

	/* Create label */
	struct scaled_font_buffer *label_buffer = scaled_font_buffer_create(tree);
	assert(label_buffer);
	scaled_font_buffer_update(label_buffer, item->text, label_max_width,
		&rc.font_menuitem, text_color, bg_color);
	/* Vertically center and left-align label */
	int x = theme->menu_items_padding_x + icon_width;
	int y = (theme->menu_item_height - label_buffer->height) / 2;
	wlr_scene_node_set_position(&label_buffer->scene_buffer->node, x, y);

	if (!item->arrow) {
		return tree;
	}

	/* Create arrow for submenu items */
	struct scaled_font_buffer *arrow_buffer = scaled_font_buffer_create(tree);
	assert(arrow_buffer);
	scaled_font_buffer_update(arrow_buffer, item->arrow, -1,
		&rc.font_menuitem, text_color, bg_color);
	/* Vertically center and right-align arrow */
	x += label_max_width;
	y = (theme->menu_item_height - label_buffer->height) / 2;
	wlr_scene_node_set_position(&arrow_buffer->scene_buffer->node, x, y);

	return tree;
}

static void
item_create_scene(struct menuitem *menuitem, int *item_y)
{
	assert(menuitem);
	assert(menuitem->type == LAB_MENU_ITEM);
	struct menu *menu = menuitem->parent;
	struct theme *theme = menu->server->theme;

	/* Menu item root node */
	menuitem->tree = wlr_scene_tree_create(menu->scene_tree);
	node_descriptor_create(&menuitem->tree->node,
		LAB_NODE_DESC_MENUITEM, menuitem);

	/* Create scenes for unselected/selected states */
	menuitem->normal_tree = item_create_scene_for_state(menuitem,
		theme->menu_items_text_color,
		theme->menu_items_bg_color);
	menuitem->selected_tree = item_create_scene_for_state(menuitem,
		theme->menu_items_active_text_color,
		theme->menu_items_active_bg_color);
	/* Hide selected state */
	wlr_scene_node_set_enabled(&menuitem->selected_tree->node, false);

	/* Position the item in relation to its menu */
	wlr_scene_node_set_position(&menuitem->tree->node,
		theme->menu_border_width, *item_y);
	*item_y += theme->menu_item_height;
}

static struct menuitem *
separator_create(struct menu *menu, const char *label)
{
	struct menuitem *menuitem = znew(*menuitem);
	menuitem->parent = menu;
	menuitem->selectable = false;
	menuitem->type = string_null_or_empty(label) ? LAB_MENU_SEPARATOR_LINE
		: LAB_MENU_TITLE;
	if (menuitem->type == LAB_MENU_TITLE) {
		menuitem->text = xstrdup(label);
		menuitem->native_width = font_width(&rc.font_menuheader, label);
	}

	wl_list_append(&menu->menuitems, &menuitem->link);
	wl_list_init(&menuitem->actions);
	return menuitem;
}

static void
separator_create_scene(struct menuitem *menuitem, int *item_y)
{
	assert(menuitem);
	assert(menuitem->type == LAB_MENU_SEPARATOR_LINE);
	struct menu *menu = menuitem->parent;
	struct theme *theme = menu->server->theme;

	/* Menu item root node */
	menuitem->tree = wlr_scene_tree_create(menu->scene_tree);
	node_descriptor_create(&menuitem->tree->node,
		LAB_NODE_DESC_MENUITEM, menuitem);

	/* Tree to hold background and line buffer */
	menuitem->normal_tree = wlr_scene_tree_create(menuitem->tree);

	int bg_height = theme->menu_separator_line_thickness
		+ 2 * theme->menu_separator_padding_height;
	int bg_width = menu->size.width - 2 * theme->menu_border_width;
	int line_width = bg_width - 2 * theme->menu_separator_padding_width;

	if (line_width <= 0) {
		wlr_log(WLR_ERROR, "not enough space for menu separator");
		goto error;
	}

	/* Item background nodes */
	wlr_scene_rect_create(menuitem->normal_tree, bg_width, bg_height,
		theme->menu_items_bg_color);

	/* Draw separator line */
	struct wlr_scene_rect *line_rect = wlr_scene_rect_create(
		menuitem->normal_tree, line_width,
		theme->menu_separator_line_thickness,
		theme->menu_separator_color);

	/* Vertically center-align separator line */
	wlr_scene_node_set_position(&line_rect->node,
		theme->menu_separator_padding_width,
		theme->menu_separator_padding_height);
error:
	wlr_scene_node_set_position(&menuitem->tree->node,
		theme->menu_border_width, *item_y);
	*item_y += bg_height;
}

static void
title_create_scene(struct menuitem *menuitem, int *item_y)
{
	assert(menuitem);
	assert(menuitem->type == LAB_MENU_TITLE);
	struct menu *menu = menuitem->parent;
	struct theme *theme = menu->server->theme;
	float *bg_color = theme->menu_title_bg_color;
	float *text_color = theme->menu_title_text_color;

	/* Menu item root node */
	menuitem->tree = wlr_scene_tree_create(menu->scene_tree);
	node_descriptor_create(&menuitem->tree->node,
		LAB_NODE_DESC_MENUITEM, menuitem);

	/* Tree to hold background and text buffer */
	menuitem->normal_tree = wlr_scene_tree_create(menuitem->tree);

	int bg_width = menu->size.width - 2 * theme->menu_border_width;
	int text_width = bg_width - 2 * theme->menu_items_padding_x;

	if (text_width <= 0) {
		wlr_log(WLR_ERROR, "not enough space for menu title");
		goto error;
	}

	/* Background */
	wlr_scene_rect_create(menuitem->normal_tree,
		bg_width, theme->menu_header_height, bg_color);

	/* Draw separator title */
	struct scaled_font_buffer *title_font_buffer =
		scaled_font_buffer_create(menuitem->normal_tree);
	assert(title_font_buffer);
	scaled_font_buffer_update(title_font_buffer, menuitem->text,
		text_width, &rc.font_menuheader, text_color, bg_color);

	int title_x = 0;
	switch (theme->menu_title_text_justify) {
	case LAB_JUSTIFY_CENTER:
		title_x = (bg_width - menuitem->native_width) / 2;
		title_x = MAX(title_x, 0);
		break;
	case LAB_JUSTIFY_LEFT:
		title_x = theme->menu_items_padding_x;
		break;
	case LAB_JUSTIFY_RIGHT:
		title_x = bg_width - menuitem->native_width
				- theme->menu_items_padding_x;
		break;
	}
	int title_y = (theme->menu_header_height - title_font_buffer->height) / 2;
	wlr_scene_node_set_position(&title_font_buffer->scene_buffer->node,
		title_x, title_y);
error:
	wlr_scene_node_set_position(&menuitem->tree->node,
		theme->menu_border_width, *item_y);
	*item_y += theme->menu_header_height;
}

static void item_destroy(struct menuitem *item);

static void
reset_menu(struct menu *menu)
{
	struct menuitem *item, *next;
	wl_list_for_each_safe(item, next, &menu->menuitems, link) {
		item_destroy(item);
	}
	if (menu->scene_tree) {
		wlr_scene_node_destroy(&menu->scene_tree->node);
		menu->scene_tree = NULL;
	}
	/* TODO: also reset other fields? */
}

static void
menu_create_scene(struct menu *menu)
{
	struct menuitem *item;
	struct theme *theme = menu->server->theme;

	assert(!menu->scene_tree);

	menu->scene_tree = wlr_scene_tree_create(menu->server->menu_tree);
	wlr_scene_node_set_enabled(&menu->scene_tree->node, false);

	/* Menu width is the maximum item width, capped by menu.width.{min,max} */
	menu->size.width = 0;
	wl_list_for_each(item, &menu->menuitems, link) {
		int width = item->native_width
			+ 2 * theme->menu_items_padding_x
			+ 2 * theme->menu_border_width;
		menu->size.width = MAX(menu->size.width, width);
	}

	if (menu->has_icons) {
		menu->size.width += theme->menu_items_padding_x + ICON_SIZE;
	}
	menu->size.width = MAX(menu->size.width, theme->menu_min_width);
	menu->size.width = MIN(menu->size.width, theme->menu_max_width);

	/* Update all items for the new size */
	int item_y = theme->menu_border_width;
	wl_list_for_each(item, &menu->menuitems, link) {
		assert(!item->tree);
		switch (item->type) {
		case LAB_MENU_ITEM:
			item_create_scene(item, &item_y);
			break;
		case LAB_MENU_SEPARATOR_LINE:
			separator_create_scene(item, &item_y);
			break;
		case LAB_MENU_TITLE:
			title_create_scene(item, &item_y);
			break;
		}
	}
	menu->size.height = item_y + theme->menu_border_width;

	float transparent[4] = {0};
	struct scaled_rect_buffer *bg_buffer = scaled_rect_buffer_create(
		menu->scene_tree, menu->size.width, menu->size.height,
		theme->menu_border_width, transparent,
		theme->menu_border_color);
	assert(bg_buffer);
	wlr_scene_node_lower_to_bottom(&bg_buffer->scene_buffer->node);
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
	/*
	 * Nodenames for most menu-items end with '.item.menu' but top-level
	 * pipemenu items do not have the associated <menu> element so merely
	 * end with a '.item'
	 */
	string_truncate_at_pattern(nodename, ".item.menu");
	string_truncate_at_pattern(nodename, ".item");

	/* <item label=""> defines the start of a new item */
	if (!strcmp(nodename, "label")) {
		current_item = item_create(current_menu, content, false);
		current_item_action = NULL;
	} else if (!current_item) {
		wlr_log(WLR_ERROR, "expect <item label=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcmp(nodename, "icon")) {
#if HAVE_LIBSFDO
		if (rc.menu_show_icons && !string_null_or_empty(content)) {
			xstrdup_replace(current_item->icon_name, content);
			current_menu->has_icons = true;
		}
#endif
	} else if (!strcmp(nodename, "name.action")) {
		current_item_action = action_create(content);
		if (current_item_action) {
			wl_list_append(&current_item->actions,
				&current_item_action->link);
		}
	} else if (!current_item_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(current_item_action, nodename, content);
	}
}

static void
item_destroy(struct menuitem *item)
{
	wl_list_remove(&item->link);
	action_list_free(&item->actions);
	if (item->tree) {
		wlr_scene_node_destroy(&item->tree->node);
	}
	free(item->text);
	free(item->icon_name);
	free(item);
}

/*
 * We support XML CDATA for <command> in menu.xml in order to provide backward
 * compatibility with obmenu-generator. For example:
 *
 * <menu id="" label="">
 *   <item label="">
 *     <action name="Execute">
 *       <command><![CDATA[xdg-open .]]></command>
 *     </action>
 *   </item>
 * </menu>
 *
 * <execute> is an old, deprecated openbox variety of <command>. We support it
 * for backward compatibility with old openbox-menu generators. It has the same
 * function and <command>
 *
 * The following nodenames support CDATA.
 *  - command.action.item.*menu.openbox_menu
 *  - execute.action.item.*menu.openbox_menu
 *  - command.action.item.openbox_pipe_menu
 *  - execute.action.item.openbox_pipe_menu
 *  - command.action.item.*menu.openbox_pipe_menu
 *  - execute.action.item.*menu.openbox_pipe_menu
 *
 * The *menu allows nested menus with nodenames such as ...menu.menu... or
 * ...menu.menu.menu... and so on. We could use match_glob() for all of the
 * above but it seems simpler to just check the first three fields.
 */
static bool
nodename_supports_cdata(char *nodename)
{
	return !strncmp("command.action.", nodename, 15)
		|| !strncmp("execute.action.", nodename, 15);
}

static void
entry(xmlNode *node, char *nodename, char *content)
{
	if (!nodename) {
		return;
	}
	xmlChar *cdata = NULL;
	if (!content && nodename_supports_cdata(nodename)) {
		cdata = xmlNodeGetContent(node);
	}
	if (!content && !cdata) {
		return;
	}
	string_truncate_at_pattern(nodename, ".openbox_menu");
	string_truncate_at_pattern(nodename, ".openbox_pipe_menu");
	if (getenv("LABWC_DEBUG_MENU_NODENAMES")) {
		printf("%s: %s\n", nodename, content ? content : (char *)cdata);
	}
	if (in_item) {
		fill_item(nodename, content ? content : (char *)cdata);
	}
	xmlFree(cdata);
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

static bool parse_buf(struct server *server, struct buf *buf);
static int handle_pipemenu_readable(int fd, uint32_t mask, void *_ctx);
static int handle_pipemenu_timeout(void *_ctx);

/*
 * <menu> elements have three different roles:
 *  * Definition of (sub)menu - has ID, LABEL and CONTENT
 *  * Menuitem of pipemenu type - has ID, LABEL and EXECUTE
 *  * Menuitem of submenu type - has ID only
 */
static void
handle_menu_element(xmlNode *n, struct server *server)
{
	char *label = (char *)xmlGetProp(n, (const xmlChar *)"label");
	char *icon_name = (char *)xmlGetProp(n, (const xmlChar *)"icon");
	char *execute = (char *)xmlGetProp(n, (const xmlChar *)"execute");
	char *id = (char *)xmlGetProp(n, (const xmlChar *)"id");

	if (!id) {
		wlr_log(WLR_ERROR, "<menu> without id is not allowed");
		goto error;
	}

	if (execute && label) {
		wlr_log(WLR_DEBUG, "pipemenu '%s:%s:%s'", id, label, execute);

		struct menu *pipemenu = menu_create(server, id, label);
		pipemenu->execute = xstrdup(execute);
		if (!current_menu) {
			/*
			 * A pipemenu may not have its parent like:
			 *
			 * <?xml version="1.0" encoding="UTF-8"?>
			 * <openbox_menu>
			 *   <menu id="root-menu" label="foo" execute="bar"/>
			 * </openbox_menu>
			 */
		} else {
			current_item = item_create(current_menu, label,
				/* arrow */ true);
			fill_item("icon", icon_name);
			current_item_action = NULL;
			current_item->submenu = pipemenu;
		}
	} else if ((label && current_menu) || !current_menu) {
		/*
		 * (label && current_menu) refers to <menu id="" label="">
		 * which is an nested (inline) menu definition.
		 *
		 * (!current_menu) catches:
		 *     <openbox_menu>
		 *       <menu id=""></menu>
		 *     </openbox_menu>
		 * or
		 *     <openbox_menu>
		 *       <menu id="" label=""></menu>
		 *     </openbox_menu>
		 *
		 * which is the highest level a menu can be defined at.
		 *
		 * Openbox spec requires a label="" defined here, but it is
		 * actually pointless so we handle it with or without the label
		 * attribute to make it easier for users to define "root-menu"
		 * and "client-menu".
		 */
		struct menu *parent_menu = current_menu;
		current_menu = menu_create(server, id, label);
		if (icon_name) {
			current_menu->icon_name = xstrdup(icon_name);
		}
		if (label && parent_menu) {
			/*
			 * In a nested (inline) menu definition we need to
			 * create an item pointing to the new submenu
			 */
			current_item = item_create(parent_menu, label, true);
			fill_item("icon", icon_name);
			current_item->submenu = current_menu;
		}
		traverse(n, server);
		current_menu = parent_menu;
	} else {
		/*
		 * <menu id=""> (when inside another <menu> element) creates an
		 * entry which points to a menu defined elsewhere.
		 *
		 * This is only supported in static menus. Pipemenus need to use
		 * nested (inline) menu definitions, otherwise we could have a
		 * pipemenu opening the "root-menu" or similar.
		 */

		if (waiting_for_pipe_menu) {
			wlr_log(WLR_ERROR,
				"cannot link to static menu from pipemenu");
			goto error;
		}

		struct menu *menu = menu_get_by_id(server, id);
		if (!menu) {
			wlr_log(WLR_ERROR, "no menu with id '%s'", id);
			goto error;
		}

		struct menu *iter = current_menu;
		while (iter) {
			if (iter == menu) {
				wlr_log(WLR_ERROR, "menus with the same id '%s' "
					"cannot be nested", id);
				goto error;
			}
			iter = iter->parent;
		}

		current_item = item_create(current_menu, menu->label, true);
		fill_item("icon", menu->icon_name);
		current_item->submenu = menu;
	}
error:
	free(label);
	free(icon_name);
	free(execute);
	free(id);
}

/* This can be one of <separator> and <separator label=""> */
static void
handle_separator_element(xmlNode *n)
{
	char *label = (char *)xmlGetProp(n, (const xmlChar *)"label");
	current_item = separator_create(current_menu, label);
	free(label);
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
		if (!strcasecmp((char *)n->name, "separator")) {
			handle_separator_element(n);
			continue;
		}
		if (!strcasecmp((char *)n->name, "item")) {
			if (!current_menu) {
				wlr_log(WLR_ERROR,
					"ignoring <item> without parent <menu>");
				continue;
			}
			in_item = true;
			traverse(n, server);
			in_item = false;
			continue;
		}
		traverse(n, server);
	}
}

static bool
parse_buf(struct server *server, struct buf *buf)
{
	int options = 0;
	xmlDoc *d = xmlReadMemory(buf->data, buf->len, NULL, NULL, options);
	if (!d) {
		wlr_log(WLR_ERROR, "xmlParseMemory()");
		return false;
	}
	xml_tree_walk(xmlDocGetRootElement(d), server);
	xmlFreeDoc(d);
	xmlCleanupParser();
	return true;
}

/*
 * @stream can come from either of the following:
 *   - fopen() in the case of reading a file such as menu.xml
 *   - popen() when processing pipemenus
 */
static void
parse_stream(struct server *server, FILE *stream)
{
	char *line = NULL;
	size_t len = 0;
	struct buf b = BUF_INIT;

	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		buf_add(&b, line);
	}
	free(line);
	parse_buf(server, &b);
	buf_reset(&b);
}

static void
parse_xml(const char *filename, struct server *server)
{
	struct wl_list paths;
	paths_config_create(&paths, filename);

	bool should_merge_config = rc.merge_config;
	struct wl_list *(*iter)(struct wl_list *list);
	iter = should_merge_config ? paths_get_prev : paths_get_next;

	for (struct wl_list *elm = iter(&paths); elm != &paths; elm = iter(elm)) {
		struct path *path = wl_container_of(elm, path, link);
		FILE *stream = fopen(path->string, "r");
		if (!stream) {
			continue;
		}
		wlr_log(WLR_INFO, "read menu file %s", path->string);
		parse_stream(server, stream);
		fclose(stream);
		if (!should_merge_config) {
			break;
		}
	}
	paths_destroy(&paths);
}

/*
 * Returns the box of a menuitem next to which its submenu is opened.
 * This box can be shrunk or expanded by menu overlaps and borders.
 */
static struct wlr_box
get_item_anchor_rect(struct theme *theme, struct menuitem *item)
{
	struct menu *menu = item->parent;
	int menu_x = menu->scene_tree->node.x;
	int menu_y = menu->scene_tree->node.y;
	int overlap_x = theme->menu_overlap_x + theme->menu_border_width;
	int overlap_y = theme->menu_overlap_y - theme->menu_border_width;
	return (struct wlr_box) {
		.x = menu_x + overlap_x,
		.y = menu_y + item->tree->node.y + overlap_y,
		.width = menu->size.width - 2 * overlap_x,
		.height = theme->menu_item_height - 2 * overlap_y,
	};
}

static void
menu_reposition(struct menu *menu, struct wlr_box anchor_rect)
{
	/* Get output usable area to place the menu within */
	struct output *output = output_nearest_to(menu->server,
		anchor_rect.x, anchor_rect.y);
	if (!output) {
		wlr_log(WLR_ERROR, "no output found around (%d,%d)",
			anchor_rect.x, anchor_rect.y);
		return;
	}
	struct wlr_box usable = output_usable_area_in_layout_coords(output);

	/* Policy for menu placement */
	struct wlr_xdg_positioner_rules rules = {0};
	rules.size.width = menu->size.width;
	rules.size.height = menu->size.height;
	/* A rectangle next to which the menu is opened */
	rules.anchor_rect = anchor_rect;
	/*
	 * Place menu at left or right side of anchor_rect, with their
	 * top edges aligned. The alignment is inherited from parent.
	 */
	if (menu->parent && menu->parent->align_left) {
		rules.anchor = XDG_POSITIONER_ANCHOR_TOP_LEFT;
		rules.gravity = XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
	} else {
		rules.anchor = XDG_POSITIONER_ANCHOR_TOP_RIGHT;
		rules.gravity = XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
	}
	/* Flip or slide the menu when it overflows from the output */
	rules.constraint_adjustment =
		XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X
		| XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X
		| XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
	if (!menu->parent) {
		/* Allow vertically flipping the root menu */
		rules.constraint_adjustment |=
			XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
	}

	struct wlr_box box;
	wlr_xdg_positioner_rules_get_geometry(&rules, &box);
	wlr_xdg_positioner_rules_unconstrain_box(&rules, &usable, &box);
	wlr_scene_node_set_position(&menu->scene_tree->node, box.x, box.y);

	menu->align_left = (box.x < anchor_rect.x);
}

static void
menu_hide_submenu(struct server *server, const char *id)
{
	struct menu *menu, *hide_menu;
	hide_menu = menu_get_by_id(server, id);
	if (!hide_menu) {
		return;
	}
	wl_list_for_each(menu, &server->menus, link) {
		struct menuitem *item, *next;
		wl_list_for_each_safe(item, next, &menu->menuitems, link) {
			if (item->submenu == hide_menu) {
				item_destroy(item);
			}
		}
	}
}

static void
init_client_send_to_menu(struct server *server)
{
	/* Just create placeholder. Contents will be created when launched */
	menu_create(server, "client-send-to-menu", "");
}

/*
 * This is client-send-to-menu
 * an internal menu similar to root-menu and client-menu
 *
 * This will look at workspaces and produce a menu
 * with the workspace names that can be used with
 * SendToDesktop, left/right options are included.
 */
static void
update_client_send_to_menu(struct server *server)
{
	struct menu *menu = menu_get_by_id(server, "client-send-to-menu");
	assert(menu);

	reset_menu(menu);

	struct workspace *workspace;

	wl_list_for_each(workspace, &server->workspaces.all, link) {
		if (workspace == server->workspaces.current) {
			char *label = strdup_printf(">%s<", workspace->name);
			current_item = item_create(menu, label,
				/*show arrow*/ false);
			free(label);
		} else {
			current_item = item_create(menu, workspace->name, /*show arrow*/ false);
		}
		fill_item("name.action", "SendToDesktop");
		fill_item("to.action", workspace->name);
	}

	menu_create_scene(menu);
}

static void
init_client_list_combined_menu(struct server *server)
{
	/* Just create placeholder. Contents will be created when launched */
	menu_create(server, "client-list-combined-menu", "");
}

/*
 * This is client-list-combined-menu an internal menu similar to root-menu and
 * client-menu.
 *
 * This will look at workspaces and produce a menu with the workspace name as a
 * separator label and the titles of the view, if any, below each workspace
 * name. Active view is indicated by "*" preceding title.
 */
static void
update_client_list_combined_menu(struct server *server)
{
	struct menu *menu = menu_get_by_id(server, "client-list-combined-menu");
	assert(menu);

	reset_menu(menu);

	struct workspace *workspace;
	struct view *view;
	struct buf buffer = BUF_INIT;

	wl_list_for_each(workspace, &server->workspaces.all, link) {
		buf_add_fmt(&buffer, workspace == server->workspaces.current ? ">%s<" : "%s",
				workspace->name);
		current_item = separator_create(menu, buffer.data);
		buf_clear(&buffer);

		wl_list_for_each(view, &server->views, link) {
			if (view->workspace == workspace) {
				const char *title = view_get_string_prop(view, "title");
				if (!view->foreign_toplevel || string_null_or_empty(title)) {
					continue;
				}

				if (view == server->active_view) {
					buf_add(&buffer, "*");
				}
				buf_add(&buffer, title);

				current_item = item_create(menu, buffer.data, /*show arrow*/ false);
				current_item->client_list_view = view;
				fill_item("name.action", "Focus");
				fill_item("name.action", "Raise");
				buf_clear(&buffer);
				menu->has_icons = true;
			}
		}
		current_item = item_create(menu, _("Go there..."), /*show arrow*/ false);
		fill_item("name.action", "GoToDesktop");
		fill_item("to.action", workspace->name);
	}
	buf_reset(&buffer);
	menu_create_scene(menu);
}

static void
init_rootmenu(struct server *server)
{
	struct menu *menu = menu_get_by_id(server, "root-menu");

	/* Default menu if no menu.xml found */
	if (!menu) {
		current_menu = NULL;
		menu = menu_create(server, "root-menu", "");
		current_item = item_create(menu, _("Reconfigure"), false);
		fill_item("name.action", "Reconfigure");
		current_item = item_create(menu, _("Exit"), false);
		fill_item("name.action", "Exit");
	}
}

static void
init_windowmenu(struct server *server)
{
	struct menu *menu = menu_get_by_id(server, "client-menu");

	/* Default menu if no menu.xml found */
	if (!menu) {
		current_menu = NULL;
		menu = menu_create(server, "client-menu", "");
		current_item = item_create(menu, _("Minimize"), false);
		fill_item("name.action", "Iconify");
		current_item = item_create(menu, _("Maximize"), false);
		fill_item("name.action", "ToggleMaximize");
		current_item = item_create(menu, _("Fullscreen"), false);
		fill_item("name.action", "ToggleFullscreen");
		current_item = item_create(menu, _("Roll Up/Down"), false);
		fill_item("name.action", "ToggleShade");
		current_item = item_create(menu, _("Decorations"), false);
		fill_item("name.action", "ToggleDecorations");
		current_item = item_create(menu, _("Always on Top"), false);
		fill_item("name.action", "ToggleAlwaysOnTop");

		/* Workspace sub-menu */
		struct menu *workspace_menu = menu_create(server, "workspaces", "");
		current_item = item_create(workspace_menu, _("Move Left"), false);
		/*
		 * <action name="SendToDesktop"><follow> is true by default so
		 * GoToDesktop will be called as part of the action.
		 */
		fill_item("name.action", "SendToDesktop");
		fill_item("to.action", "left");
		current_item = item_create(workspace_menu, _("Move Right"), false);
		fill_item("name.action", "SendToDesktop");
		fill_item("to.action", "right");
		current_item = separator_create(workspace_menu, "");
		current_item = item_create(workspace_menu,
			_("Always on Visible Workspace"), false);
		fill_item("name.action", "ToggleOmnipresent");

		current_item = item_create(menu, _("Workspace"), true);
		current_item->submenu = workspace_menu;

		current_item = item_create(menu, _("Close"), false);
		fill_item("name.action", "Close");
	}

	if (wl_list_length(&rc.workspace_config.workspaces) == 1) {
		menu_hide_submenu(server, "workspaces");
	}
}

void
menu_init(struct server *server)
{
	wl_list_init(&server->menus);
	parse_xml("menu.xml", server);
	init_rootmenu(server);
	init_windowmenu(server);
	init_client_list_combined_menu(server);
	init_client_send_to_menu(server);
	validate(server);
}

static void
nullify_item_pointing_to_this_menu(struct menu *menu)
{
	struct menu *iter;
	wl_list_for_each(iter, &menu->server->menus, link) {
		struct menuitem *item;
		wl_list_for_each(item, &iter->menuitems, link) {
			if (item->submenu == menu) {
				item->submenu = NULL;
				/*
				 * Let's not return early here in case we have
				 * multiple items pointing to the same menu.
				 */
			}
		}

		/* This is important for pipe-menus */
		if (iter->parent == menu) {
			iter->parent = NULL;
		}
	}
}

static void pipemenu_ctx_destroy(struct menu_pipe_context *ctx);

static void
menu_free(struct menu *menu)
{
	/* Keep items clean on pipemenu destruction */
	nullify_item_pointing_to_this_menu(menu);

	if (menu->server->menu_current == menu) {
		menu_close_root(menu->server);
	}

	struct menuitem *item, *next;
	wl_list_for_each_safe(item, next, &menu->menuitems, link) {
		item_destroy(item);
	}

	if (menu->pipe_ctx) {
		pipemenu_ctx_destroy(menu->pipe_ctx);
		assert(!menu->pipe_ctx);
	}

	/*
	 * Destroying the root node will destroy everything,
	 * including node descriptors and scaled_font_buffers.
	 */
	if (menu->scene_tree) {
		wlr_scene_node_destroy(&menu->scene_tree->node);
	}
	wl_list_remove(&menu->link);
	zfree(menu->id);
	zfree(menu->label);
	zfree(menu->icon_name);
	zfree(menu->execute);
	zfree(menu);
}

void
menu_finish(struct server *server)
{
	struct menu *menu, *tmp_menu;
	wl_list_for_each_safe(menu, tmp_menu, &server->menus, link) {
		menu_free(menu);
	}

	/* Reset state vars for starting fresh when Reload is triggered */
	current_item = NULL;
	current_item_action = NULL;
	current_menu = NULL;
}

void
menu_on_view_destroy(struct view *view)
{
	struct server *server = view->server;

	/* If the view being destroy has an open window menu, then close it */
	if (server->menu_current
			&& server->menu_current->triggered_by_view == view) {
		menu_close_root(server);
	}

	/*
	 * TODO: Instead of just setting client_list_view to NULL and deleting
	 * the actions (as below), consider destroying the item and somehow
	 * updating the menu and its selection state.
	 */

	/* Also nullify the destroyed view in client-list-combined-menu */
	struct menu *menu = menu_get_by_id(server, "client-list-combined-menu");
	if (menu) {
		struct menuitem *item;
		wl_list_for_each(item, &menu->menuitems, link) {
			if (item->client_list_view == view) {
				item->client_list_view = NULL;
				action_list_free(&item->actions);
			}
		}
	}
}

/* Sets selection (or clears selection if passing NULL) */
static void
menu_set_selection(struct menu *menu, struct menuitem *item)
{
	/* Clear old selection */
	if (menu->selection.item) {
		wlr_scene_node_set_enabled(
			&menu->selection.item->normal_tree->node, true);
		wlr_scene_node_set_enabled(
			&menu->selection.item->selected_tree->node, false);
	}
	/* Set new selection */
	if (item) {
		wlr_scene_node_set_enabled(&item->normal_tree->node, false);
		wlr_scene_node_set_enabled(&item->selected_tree->node, true);
	}
	menu->selection.item = item;
}

/*
 * We only destroy pipemenus when closing the entire menu-tree so that pipemenu
 * are cached (for as long as the menu is open). This drastically improves the
 * felt performance when interacting with multiple pipe menus where a single
 * item may be selected multiple times.
 */
static void
reset_pipemenus(struct server *server)
{
	wlr_log(WLR_DEBUG, "number of menus before close=%d",
		wl_list_length(&server->menus));

	struct menu *iter, *tmp;
	wl_list_for_each_safe(iter, tmp, &server->menus, link) {
		if (iter->is_pipemenu_child) {
			/* Destroy submenus of pipemenus */
			menu_free(iter);
		} else if (iter->execute) {
			/*
			 * Destroy items and scene-nodes of pipemenus so that
			 * they are generated again when being opened
			 */
			reset_menu(iter);
		}
	}

	wlr_log(WLR_DEBUG, "number of menus after  close=%d",
		wl_list_length(&server->menus));
}

static void
_close(struct menu *menu)
{
	if (menu->scene_tree) {
		wlr_scene_node_set_enabled(&menu->scene_tree->node, false);
	}
	menu_set_selection(menu, NULL);
	if (menu->selection.menu) {
		_close(menu->selection.menu);
		menu->selection.menu = NULL;
	}
	if (menu->pipe_ctx) {
		pipemenu_ctx_destroy(menu->pipe_ctx);
		assert(!menu->pipe_ctx);
	}
}

static void
menu_close(struct menu *menu)
{
	if (!menu) {
		wlr_log(WLR_ERROR, "Trying to close non exiting menu");
		return;
	}
	_close(menu);
}

static void
open_menu(struct menu *menu, struct wlr_box anchor_rect)
{
	if (!strcmp(menu->id, "client-list-combined-menu")) {
		update_client_list_combined_menu(menu->server);
	} else if (!strcmp(menu->id, "client-send-to-menu")) {
		update_client_send_to_menu(menu->server);
	}

	if (!menu->scene_tree) {
		menu_create_scene(menu);
		assert(menu->scene_tree);
	}
	menu_reposition(menu, anchor_rect);
	wlr_scene_node_set_enabled(&menu->scene_tree->node, true);
}

static void open_pipemenu_async(struct menu *pipemenu, struct wlr_box anchor_rect);

void
menu_open_root(struct menu *menu, int x, int y)
{
	assert(menu);

	if (menu->server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		return;
	}

	assert(!menu->server->menu_current);

	struct wlr_box anchor_rect = {.x = x, .y = y};
	if (menu->execute) {
		open_pipemenu_async(menu, anchor_rect);
	} else {
		open_menu(menu, anchor_rect);
	}

	menu->server->menu_current = menu;
	selected_item = NULL;
	seat_focus_override_begin(&menu->server->seat,
		LAB_INPUT_STATE_MENU, LAB_CURSOR_DEFAULT);
}

static void
create_pipe_menu(struct menu_pipe_context *ctx)
{
	struct server *server = ctx->pipemenu->server;
	struct menu *old_current_menu = current_menu;
	current_menu = ctx->pipemenu;
	if (!parse_buf(server, &ctx->buf)) {
		goto restore_menus;
	}
	/* TODO: apply validate() only for generated pipemenus */
	validate(server);

	/* Finally open the new submenu tree */
	open_menu(ctx->pipemenu, ctx->anchor_rect);

restore_menus:
	current_menu = old_current_menu;
}

static void
pipemenu_ctx_destroy(struct menu_pipe_context *ctx)
{
	wl_event_source_remove(ctx->event_read);
	wl_event_source_remove(ctx->event_timeout);
	spawn_piped_close(ctx->pid, ctx->pipe_fd);
	buf_reset(&ctx->buf);
	if (ctx->pipemenu) {
		ctx->pipemenu->pipe_ctx = NULL;
	}
	free(ctx);
	waiting_for_pipe_menu = false;
}

static int
handle_pipemenu_timeout(void *_ctx)
{
	struct menu_pipe_context *ctx = _ctx;
	wlr_log(WLR_ERROR, "[pipemenu %ld] timeout reached, killing %s",
		(long)ctx->pid, ctx->pipemenu->execute);
	kill(ctx->pid, SIGTERM);
	pipemenu_ctx_destroy(ctx);
	return 0;
}

static int
handle_pipemenu_readable(int fd, uint32_t mask, void *_ctx)
{
	struct menu_pipe_context *ctx = _ctx;
	/* two 4k pages + 1 NULL byte */
	char data[8193];
	ssize_t size;

	do {
		/* leave space for terminating NULL byte */
		size = read(fd, data, sizeof(data) - 1);
	} while (size == -1 && errno == EINTR);

	if (size == -1) {
		wlr_log_errno(WLR_ERROR, "[pipemenu %ld] failed to read data (%s)",
			(long)ctx->pid, ctx->pipemenu->execute);
		goto clean_up;
	}

	/* Limit pipemenu buffer to 1 MiB for safety */
	if (ctx->buf.len + size > PIPEMENU_MAX_BUF_SIZE) {
		wlr_log(WLR_ERROR, "[pipemenu %ld] too big (> %d bytes); killing %s",
			(long)ctx->pid, PIPEMENU_MAX_BUF_SIZE,
			ctx->pipemenu->execute);
		kill(ctx->pid, SIGTERM);
		goto clean_up;
	}

	wlr_log(WLR_DEBUG, "[pipemenu %ld] read %ld bytes of data", (long)ctx->pid, size);
	if (size) {
		data[size] = '\0';
		buf_add(&ctx->buf, data);
		return 0;
	}

	/* Guard against badly formed data such as binary input */
	if (!str_starts_with(ctx->buf.data, '<', " \t\r\n")) {
		wlr_log(WLR_ERROR, "expect xml data to start with '<'; abort pipemenu");
		goto clean_up;
	}

	create_pipe_menu(ctx);

clean_up:
	pipemenu_ctx_destroy(ctx);
	return 0;
}

static void
open_pipemenu_async(struct menu *pipemenu, struct wlr_box anchor_rect)
{
	struct server *server = pipemenu->server;

	assert(!pipemenu->pipe_ctx);
	assert(!pipemenu->scene_tree);

	int pipe_fd = 0;
	pid_t pid = spawn_piped(pipemenu->execute, &pipe_fd);
	if (pid <= 0) {
		wlr_log(WLR_ERROR, "Failed to spawn pipe menu process %s",
			pipemenu->execute);
		return;
	}

	waiting_for_pipe_menu = true;
	struct menu_pipe_context *ctx = znew(*ctx);
	ctx->pid = pid;
	ctx->pipe_fd = pipe_fd;
	ctx->buf = BUF_INIT;
	ctx->anchor_rect = anchor_rect;
	ctx->pipemenu = pipemenu;
	pipemenu->pipe_ctx = ctx;

	ctx->event_read = wl_event_loop_add_fd(server->wl_event_loop,
		pipe_fd, WL_EVENT_READABLE, handle_pipemenu_readable, ctx);

	ctx->event_timeout = wl_event_loop_add_timer(server->wl_event_loop,
		handle_pipemenu_timeout, ctx);
	wl_event_source_timer_update(ctx->event_timeout, PIPEMENU_TIMEOUT_IN_MS);

	wlr_log(WLR_DEBUG, "[pipemenu %ld] executed: %s",
		(long)ctx->pid, ctx->pipemenu->execute);
}

static void
menu_process_item_selection(struct menuitem *item)
{
	assert(item);

	/* Do not keep selecting the same item */
	if (item == selected_item) {
		return;
	}

	if (waiting_for_pipe_menu) {
		return;
	}
	selected_item = item;

	if (!item->selectable) {
		return;
	}

	/* We are on an item that has new focus */
	menu_set_selection(item->parent, item);
	if (item->parent->selection.menu) {
		/* Close old submenu tree */
		menu_close(item->parent->selection.menu);
	}

	if (item->submenu) {
		/* Sync the triggering view */
		item->submenu->triggered_by_view = item->parent->triggered_by_view;
		/* Ensure the submenu has its parent set correctly */
		item->submenu->parent = item->parent;
		/* And open the new submenu tree */
		struct wlr_box anchor_rect =
			get_item_anchor_rect(item->submenu->server->theme, item);
		if (item->submenu->execute && !item->submenu->scene_tree) {
			open_pipemenu_async(item->submenu, anchor_rect);
		} else {
			open_menu(item->submenu, anchor_rect);
		}
	}

	item->parent->selection.menu = item->submenu;
}

/* Get the deepest submenu with active item selection or the root menu itself */
static struct menu *
get_selection_leaf(struct server *server)
{
	struct menu *menu = server->menu_current;
	if (!menu) {
		return NULL;
	}

	while (menu->selection.menu) {
		if (!menu->selection.menu->selection.item) {
			return menu;
		}
		menu = menu->selection.menu;
	}

	return menu;
}

/* Selects the next or previous sibling of the currently selected item */
static void
menu_item_select(struct server *server, bool forward)
{
	struct menu *menu = get_selection_leaf(server);
	if (!menu) {
		return;
	}

	struct menuitem *item = NULL;
	struct menuitem *selection = menu->selection.item;
	struct wl_list *start = selection ? &selection->link : &menu->menuitems;
	struct wl_list *current = start;
	while (!item || !item->selectable) {
		current = forward ? current->next : current->prev;
		if (current == start) {
			return;
		}
		if (current == &menu->menuitems) {
			/* Allow wrap around */
			item = NULL;
			continue;
		}
		item = wl_container_of(current, item, link);
	}

	menu_process_item_selection(item);
}

static bool
menu_execute_item(struct menuitem *item)
{
	assert(item);

	if (item->submenu || !item->selectable) {
		/* We received a click on a separator or item that just opens a submenu */
		return false;
	}

	struct server *server = item->parent->server;
	menu_close(server->menu_current);
	server->menu_current = NULL;
	seat_focus_override_end(&server->seat);

	/*
	 * We call the actions after closing the menu so that virtual keyboard
	 * input is sent to the focused_surface instead of being absorbed by the
	 * menu. Consider for example: `wlrctl keyboard type abc`
	 *
	 * We cannot call menu_close_root() directly here because it does both
	 * menu_close() and destroy_pipemenus() which we have to handle
	 * before/after action_run() respectively.
	 */
	if (!strcmp(item->parent->id, "client-list-combined-menu")
			&& item->client_list_view) {
		actions_run(item->client_list_view, server, &item->actions, NULL);
	} else {
		actions_run(item->parent->triggered_by_view, server,
				&item->actions, NULL);
	}

	reset_pipemenus(server);
	return true;
}

/* Keyboard based selection */
void
menu_item_select_next(struct server *server)
{
	menu_item_select(server, /* forward */ true);
}

void
menu_item_select_previous(struct server *server)
{
	menu_item_select(server, /* forward */ false);
}

bool
menu_call_selected_actions(struct server *server)
{
	struct menu *menu = get_selection_leaf(server);
	if (!menu || !menu->selection.item) {
		return false;
	}

	return menu_execute_item(menu->selection.item);
}

/* Selects the first item on the submenu attached to the current selection */
void
menu_submenu_enter(struct server *server)
{
	struct menu *menu = get_selection_leaf(server);
	if (!menu || !menu->selection.menu) {
		return;
	}

	struct wl_list *start = &menu->selection.menu->menuitems;
	struct wl_list *current = start;
	struct menuitem *item = NULL;
	while (!item || !item->selectable) {
		current = current->next;
		if (current == start) {
			return;
		}
		item = wl_container_of(current, item, link);
	}

	menu_process_item_selection(item);
}

/* Re-selects the selected item on the parent menu of the current selection */
void
menu_submenu_leave(struct server *server)
{
	struct menu *menu = get_selection_leaf(server);
	if (!menu || !menu->parent || !menu->parent->selection.item) {
		return;
	}

	menu_process_item_selection(menu->parent->selection.item);
}

/* Mouse based selection */
void
menu_process_cursor_motion(struct wlr_scene_node *node)
{
	assert(node && node->data);
	struct menuitem *item = node_menuitem_from_node(node);
	menu_process_item_selection(item);
}

bool
menu_call_actions(struct wlr_scene_node *node)
{
	assert(node && node->data);
	struct menuitem *item = node_menuitem_from_node(node);

	return menu_execute_item(item);
}

void
menu_close_root(struct server *server)
{
	assert(server->input_mode == LAB_INPUT_STATE_MENU);
	assert(server->menu_current);

	menu_close(server->menu_current);
	server->menu_current = NULL;
	reset_pipemenus(server);
	seat_focus_override_end(&server->seat);
}

void
menu_reconfigure(struct server *server)
{
	menu_finish(server);
	server->menu_current = NULL;
	menu_init(server);
}
