#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "common/dir.h"
#include "common/nodename.h"
#include "common/string-helpers.h"
#include "common/zfree.h"
#include "config/keybind.h"
#include "config/mousebind.h"
#include "config/rcxml.h"

static bool in_keybind = false;
static bool is_attribute = false;
static struct keybind *current_keybind;
static const char* current_mouse_context = "";

enum font_place {
	FONT_PLACE_UNKNOWN = 0,
	FONT_PLACE_ACTIVEWINDOW,
	FONT_PLACE_MENUITEM,
	/* TODO: Add all places based on Openbox's rc.xml */
};

static void load_default_key_bindings(void);

static void
fill_keybind(char *nodename, char *content)
{
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	if (!strcmp(nodename, "key")) {
		current_keybind = keybind_create(content);
	}
	/*
	 * We expect <keybind key=""> to come first
	 * If a invalid keybind has been provided, keybind_create() complains
	 * so we just silently ignore it here.
	 */
	if (!current_keybind) {
		return;
	}
	if (!strcmp(nodename, "name.action")) {
		current_keybind->action = strdup(content);
	} else if (!strcmp(nodename, "command.action")) {
		current_keybind->command = strdup(content);
	} else if (!strcmp(nodename, "direction.action")) {
		current_keybind->command = strdup(content);
	} else if (!strcmp(nodename, "menu.action")) {
		current_keybind->command = strdup(content);
	}
}

static bool
get_bool(const char *s)
{
	if (!s) {
		return false;
	}
	if (!strcasecmp(s, "yes")) {
		return true;
	}
	if (!strcasecmp(s, "true")) {
		return true;
	}
	return false;
}

static void
fill_font(char *nodename, char *content, enum font_place place)
{
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".font.theme");

	switch (place) {
	case FONT_PLACE_UNKNOWN:
		/*
		 * If <theme><font></font></theme> is used without a place=""
		 * attribute, we set all font variables
		 */
		if (!strcmp(nodename, "name")) {
			rc.font_name_activewindow = strdup(content);
			rc.font_name_menuitem = strdup(content);
		} else if (!strcmp(nodename, "size")) {
			rc.font_size_activewindow = atoi(content);
			rc.font_size_menuitem = atoi(content);
		}
		break;
	case FONT_PLACE_ACTIVEWINDOW:
		if (!strcmp(nodename, "name")) {
			rc.font_name_activewindow = strdup(content);
		} else if (!strcmp(nodename, "size")) {
			rc.font_size_activewindow = atoi(content);
		}
		break;
	case FONT_PLACE_MENUITEM:
		if (!strcmp(nodename, "name")) {
			rc.font_name_menuitem = strdup(content);
		} else if (!strcmp(nodename, "size")) {
			rc.font_size_menuitem = atoi(content);
		}
		break;

	/* TODO: implement for all font places */

	default:
		break;
	}
}

static enum font_place
enum_font_place(const char *place)
{
	if (!place) {
		return FONT_PLACE_UNKNOWN;
	}
	if (!strcasecmp(place, "ActiveWindow")) {
		return FONT_PLACE_ACTIVEWINDOW;
	} else if (!strcasecmp(place, "MenuItem")) {
		return FONT_PLACE_MENUITEM;
	}
	return FONT_PLACE_UNKNOWN;
}

static void
entry(xmlNode *node, char *nodename, char *content)
{
	/* current <theme><font place=""></font></theme> */
	static enum font_place font_place = FONT_PLACE_UNKNOWN;

	if (!nodename) {
		return;
	}
	string_truncate_at_pattern(nodename, ".openbox_config");
	string_truncate_at_pattern(nodename, ".labwc_config");

	if (getenv("LABWC_DEBUG_CONFIG_NODENAMES")) {
		if (is_attribute) {
			printf("@");
		}
		printf("%s: %s\n", nodename, content);
	}

	/* handle nodes without content, e.g. <keyboard><default /> */
	if (!strcmp(nodename, "default.keyboard")) {
		load_default_key_bindings();
		return;
	}

	/* handle the rest */
	if (!content) {
		return;
	}
	if (in_keybind) {
		fill_keybind(nodename, content);
	}

	if (is_attribute && !strcmp(nodename, "place.font.theme")) {
		font_place = enum_font_place(content);
	}

	if (!strcmp(nodename, "decoration.core")) {
		if (!strcmp(content, "client")) {
			rc.xdg_shell_server_side_deco = false;
		} else {
			rc.xdg_shell_server_side_deco = true;
		}
	} else if (!strcmp(nodename, "gap.core")) {
		rc.gap = atoi(content);
	} else if (!strcmp(nodename, "name.theme")) {
		rc.theme_name = strdup(content);
	} else if (!strcmp(nodename, "cornerradius.theme")) {
		rc.corner_radius = atoi(content);
	} else if (!strcmp(nodename, "name.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "size.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcasecmp(nodename, "FollowMouse.focus")) {
		rc.focus_follow_mouse = get_bool(content);
	} else if (!strcasecmp(nodename, "RaiseOnFocus.focus")) {
		rc.focus_follow_mouse = true;
		rc.raise_on_focus = get_bool(content);
	}
}

static void
process_node(xmlNode *node)
{
	char *content;
	static char buffer[256];
	char *name;

	content = (char *)node->content;
	if (xmlIsBlankNode(node)) {
		return;
	}
	name = nodename(node, buffer, sizeof(buffer));
	entry(node, name, content);
}

static void xml_tree_walk(xmlNode *node);

static void
traverse(xmlNode *n)
{
	process_node(n);
	is_attribute = true;
	for (xmlAttr *attr = n->properties; attr; attr = attr->next) {
		xml_tree_walk(attr->children);
	}
	is_attribute = false;
	xml_tree_walk(n->children);
}

static bool
is_ignorable_node(const xmlNode* n)
{
	if(n->type == XML_COMMENT_NODE) {
		return true;
	}

	if(n->type == XML_TEXT_NODE) {
		for(const char* s = (const char*)n->content; s && (*s != '\0'); s++) {
			if(!isspace(*s)) {
				return false;
			} 
		}
		return true;
	}

	return false;
}

static void
traverse_mousebind_action(xmlNode* node, const char* button_str, const char* mouse_action)
{
	/*
	 * Right now, all we have implemented is:
	 *
	 * <action name="ToggleMaximize"/>             ] -- only supported attribute is "name"
	 *
	 * <action name="Execute>                    
	 *      <command>command text here</command>   ] -- if name is execute, we support a child node of name command
	 * </action>                                
	 */
	const char* action_to_do = "";
	const char* command = "";
	for(xmlAttr* attr = node->properties; attr; attr = attr->next) {
		if(strcasecmp((const char*)attr->name, "name") == 0) {
			action_to_do = (const char*)attr->children->content;
		} else {
			wlr_log(WLR_ERROR, "hit unexpected attr (%s) in mousebind action\n", (const char*) attr->name);
		}
	}

	if(strcasecmp((const char*)action_to_do, "Execute") == 0) {
		for(xmlNode* n = node->children; n && n->name; n = n->next) {
			if(strcasecmp((const char*)n->name, "command") == 0) {
				for(xmlNode* t = n->children; t; t = t->next) {
					if( (t->type == XML_TEXT_NODE) ) {
						command = (const char*)t->content;
					}
				}
			}
		}
	}
	struct mousebind* mousebind = mousebind_create(current_mouse_context,
			button_str,
			mouse_action,
			action_to_do,
			command);
	if(mousebind != NULL) {
		wl_list_insert(&rc.mousebinds, &mousebind->link);
	} else {
		wlr_log(WLR_ERROR, "failed to create mousebind:\n"
						   "    context: (%s)\n"
						   "    button:  (%s)\n"
						   "    mouse action: (%s)\n"
						   "    action: (%s)\n"
						   "    command: (%s)\n",
						   current_mouse_context,
						   button_str,
						   mouse_action,
						   action_to_do,
						   command);
	}
}

static void
traverse_mousebind(xmlNode* node)
{
    /*
     * Right now, all we have implemented is:
     *
     *  this node
     *     |          this node's only supported attributes are "button" and "action"
     *     |              |            |
     *     v              v            v
     * <mousebind button="Left" action="DoubleClick>                 
     *     <action name="ToggleMaximize"/>           -| 
     *     <action name="Execute>                     | -- This node's only supported children are actions
     *          <command>command text here</command>  |   
     *     </action>                                 -|       
     * </context>               
     */
	const char* button_str = "";
	const char* action_mouse_did_str = "";
	for(xmlAttr* attr = node->properties; attr; attr = attr->next) {
		if(strcasecmp((const char*)attr->name, "button") == 0) {
			button_str = (const char*)attr->children->content;
		} else if(strcasecmp((const char*)attr->name, "action") == 0) {
			action_mouse_did_str = (const char*)attr->children->content;
		} else {
			wlr_log(WLR_ERROR, "hit unexpected attr (%s) in mousebind\n", (const char*)attr->name);
		}
	}

	for(xmlNode* n = node->children; n && n->name; n = n->next) {
		if(strcasecmp((const char*)n->name, "action") == 0) {
			traverse_mousebind_action(n, button_str, action_mouse_did_str);
		} else if(is_ignorable_node(n)) {
			continue;
		} else {
			wlr_log(WLR_ERROR, "hit unexpected node (%s) in mousebind\n", (const char*) n->name);
		}
	}
}

static void
traverse_context(xmlNode* node)
{
    /*
     * Right now, all we have implemented is:
     *
     *  this node
     *     |          this node's only supported attribute is "name"
     *     |              |
     *     v              v
     * <context name="TitleBar">
     * 	  <mousebind....>     -|
     *          ...            | -- This node's only supported child is mousebind
     * 	  </mousebind>        -|
     * </context>
     */
	for(xmlAttr* attr = node->properties; attr; attr = attr->next) {
		if(strcasecmp((const char*)attr->name, "name") == 0) {
			current_mouse_context = (const char*)attr->children->content;
		} else {
			wlr_log(WLR_ERROR, "hit unexpected attr (%s) in context\n", (const char*)attr->name);
		}
	}

	for(xmlNode* n = node->children; n && n->name; n = n->next) {
		if(strcasecmp((const char*)n->name, "mousebind") == 0) {
			traverse_mousebind(n);
		} else if(is_ignorable_node(n)) {
			continue;
		} else {
			wlr_log(WLR_ERROR, "hit unexpected node (%s) in context\n", (const char*)n->name);
		}
	}
}

static void
traverse_doubleclick_time(xmlNode* node)
{
    /*
     *     this node
     *       |
     *       |
     *       v
     * <doubleClickTime>200</doubleClickTime>
     */
	for(xmlNode* n = node->children; n && n->name; n = n->next) {
		if(n->type == XML_TEXT_NODE) {
			long doubleclick_time_parsed = strtol((const char*)n->content, NULL, 10);

			/*
			 * There are 2 possible sources for a bad doubleclicktime value:
			 *  - user gave a value of 0 (which doesn't make sense)
			 *  - user gave a negative value (which doesn't make sense)
			 *  - user gave a value which strtol couldn't parse
			 *
			 *  since strtol() returns 0 on error, all we have to do is check
			 *  for to see if strtol() returned 0 or less to handle the error
			 *  cases. in case of error, we just choose not to override the
			 *  default value and everything should be fine
			 */
			bool valid_doubleclick_time = doubleclick_time_parsed > 0;
			if(valid_doubleclick_time) {
				rc.doubleclick_time = doubleclick_time_parsed;
			}
		}
	}
}

static void
traverse_mouse(xmlNode* node)
{
    /*
     * Right now, all we have implemented is:
     *
     *  this node
     *    |
     *    |
     *    v
     * <mouse>
     *     <doubleClickTime>200</doubleClickTime>   ]
     *     <context name="TitleBar">               -|
     *     	<mousebind....>                         |
     *           ...                                | -- This node's only supported children
     *     	</mousebind>                            |    are doubleClickTime and context
     *     </context>                              -|
     * </mouse>
     */
	for(xmlNode* n = node->children; n && n->name; n = n->next) {
		if(strcasecmp((const char*)n->name, "context") == 0) {
			traverse_context(n);
		} else if(strcasecmp((const char*)n->name, "doubleClickTime") == 0) {
			traverse_doubleclick_time(n);
		} else if(is_ignorable_node(n)) {
			continue;
		} else {
			wlr_log(WLR_ERROR, "hit unexpected node (%s) in mouse\n", (const char*)n->name);
		}
	}
}

static void
xml_tree_walk(xmlNode *node)
{
	for (xmlNode *n = node; n && n->name; n = n->next) {
		if (!strcasecmp((char *)n->name, "comment")) {
			continue;
		}
		if (!strcasecmp((char *)n->name, "keybind")) {
			in_keybind = true;
			traverse(n);
			in_keybind = false;
			continue;
		} 
		if(!strcasecmp((char *)n->name, "mouse")) {
			traverse_mouse(n);
			continue;
		}
		traverse(n);
	}
}

/* Exposed in header file to allow unit tests to parse buffers */
void
rcxml_parse_xml(struct buf *b)
{
	xmlDoc *d = xmlParseMemory(b->buf, b->len);
	if (!d) {
		wlr_log(WLR_ERROR, "xmlParseMemory()");
		exit(EXIT_FAILURE);
	}
	xml_tree_walk(xmlDocGetRootElement(d));
	xmlFreeDoc(d);
	xmlCleanupParser();
}

static void
rcxml_init()
{
	static bool has_run;

	if (has_run) {
		return;
	}
	has_run = true;
	LIBXML_TEST_VERSION
	wl_list_init(&rc.keybinds);
	wl_list_init(&rc.mousebinds);
	rc.xdg_shell_server_side_deco = true;
	rc.corner_radius = 8;
	rc.font_size_activewindow = 10;
	rc.font_size_menuitem = 10;
	rc.doubleclick_time = 500;
}

static void
bind(const char *binding, const char *action, const char *command)
{
	if (!binding || !action) {
		return;
	}
	struct keybind *k = keybind_create(binding);
	if (!k) {
		return;
	}
	if (action) {
		k->action = strdup(action);
	}
	if (command) {
		k->command = strdup(command);
	}
}

static void
load_default_key_bindings(void)
{
	bind("A-Tab", "NextWindow", NULL);
	bind("A-Escape", "Exit", NULL);
	bind("W-Return", "Execute", "alacritty");
	bind("A-F3", "Execute", "bemenu-run");
	bind("A-F4", "Close", NULL);
	bind("W-a", "ToggleMaximize", NULL);
	bind("A-Left", "MoveToEdge", "left");
	bind("A-Right", "MoveToEdge", "right");
	bind("A-Up", "MoveToEdge", "up");
	bind("A-Down", "MoveToEdge", "down");
}

static void
post_processing(void)
{
	if (!wl_list_length(&rc.keybinds)) {
		wlr_log(WLR_INFO, "load default key bindings");
		load_default_key_bindings();
	}

	if (!rc.font_name_activewindow) {
		rc.font_name_activewindow = strdup("sans");
	}
	if (!rc.font_name_menuitem) {
		rc.font_name_menuitem = strdup("sans");
	}
}

static void
rcxml_path(char *buf, size_t len)
{
	if (!strlen(config_dir())) {
		return;
	}
	snprintf(buf, len, "%s/rc.xml", config_dir());
}

static void
find_config_file(char *buffer, size_t len, const char *filename)
{
	if (filename) {
		snprintf(buffer, len, "%s", filename);
		return;
	}
	rcxml_path(buffer, len);
}

void
rcxml_read(const char *filename)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	struct buf b;
	static char rcxml[4096] = { 0 };

	rcxml_init();

	/*
	 * rcxml_read() can be called multiple times, but we only set rcxml[]
	 * the first time. The specified 'filename' is only respected the first
	 * time.
	 */
	if (rcxml[0] == '\0') {
		find_config_file(rcxml, sizeof(rcxml), filename);
	}
	if (rcxml[0] == '\0') {
		wlr_log(WLR_INFO, "cannot find rc.xml config file");
		goto no_config;
	}

	/* Reading file into buffer before parsing - better for unit tests */
	stream = fopen(rcxml, "r");
	if (!stream) {
		wlr_log(WLR_ERROR, "cannot read (%s)", rcxml);
		goto no_config;
	}
	wlr_log(WLR_INFO, "read config file %s", rcxml);
	buf_init(&b);
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p)
			*p = '\0';
		buf_add(&b, line);
	}
	free(line);
	fclose(stream);
	rcxml_parse_xml(&b);
	free(b.buf);
no_config:
	post_processing();
}

void
rcxml_finish(void)
{
	zfree(rc.font_name_activewindow);
	zfree(rc.font_name_menuitem);
	zfree(rc.theme_name);

	struct keybind *k, *k_tmp;
	wl_list_for_each_safe (k, k_tmp, &rc.keybinds, link) {
		wl_list_remove(&k->link);
		zfree(k->command);
		zfree(k->action);
		zfree(k->keysyms);
		zfree(k);
	}

	struct mousebind *m, *m_tmp;
	wl_list_for_each_safe(m, m_tmp, &rc.mousebinds, link) {
		wl_list_remove(&m->link);
		zfree(m->command);
		zfree(m->action);
		zfree(m);
	}
}
