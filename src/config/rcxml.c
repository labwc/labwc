#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wayland-server-core.h>

#include "common/bug-on.h"
#include "common/dir.h"
#include "common/font.h"
#include "common/log.h"
#include "config/keybind.h"
#include "config/rcxml.h"

static bool in_keybind = false;
static bool is_attribute = false;
static bool write_to_nodename_buffer = false;
static struct buf *nodename_buffer;
static struct keybind *current_keybind;

enum font_place {
	FONT_PLACE_UNKNOWN = 0,
	FONT_PLACE_ACTIVEWINDOW,
	FONT_PLACE_INACTIVEWINDOW,
	/* TODO: Add all places based on Openbox's rc.xml */
};

static void
rstrip(char *buf, const char *pattern)
{
	char *p = strstr(buf, pattern);
	if (!p) {
		return;
	}
	*p = '\0';
}

static void
fill_keybind(char *nodename, char *content)
{
	if (!content) {
		return;
	}
	rstrip(nodename, ".keybind.keyboard");
	if (!strcmp(nodename, "key")) {
		current_keybind = keybind_create(content);
	}
	/* We expect <keybind key=""> to come first */
	BUG_ON(!current_keybind);
	if (!strcmp(nodename, "name.action")) {
		current_keybind->action = strdup(content);
	} else if (!strcmp(nodename, "command.action")) {
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
	rstrip(nodename, ".font.theme");

	/* TODO: implement for all font places */
	if (place != FONT_PLACE_ACTIVEWINDOW) {
		return;
	}
	if (!strcmp(nodename, "name")) {
		rc.font_name_activewindow = strdup(content);
	} else if (!strcmp(nodename, "size")) {
		rc.font_size_activewindow = atoi(content);
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
	} else if (!strcasecmp(place, "InactiveWindow")) {
		return FONT_PLACE_INACTIVEWINDOW;
	}
	return FONT_PLACE_UNKNOWN;
}

static void
entry(xmlNode *node, char *nodename, char *content)
{
	/* current <theme><font place=""></theme> */
	static enum font_place font_place = FONT_PLACE_UNKNOWN;

	if (!nodename) {
		return;
	}
	rstrip(nodename, ".openbox_config");

	/* for debugging */
	if (write_to_nodename_buffer) {
		if (is_attribute) {
			buf_add(nodename_buffer, "@");
		}
		buf_add(nodename_buffer, nodename);
		if (content) {
			buf_add(nodename_buffer, ": ");
			buf_add(nodename_buffer, content);
		}
		buf_add(nodename_buffer, "\n");
	}

	if (!content) {
		return;
	}
	if (in_keybind) {
		fill_keybind(nodename, content);
	}

	if (is_attribute && !strcmp(nodename, "place.font.theme")) {
		font_place = enum_font_place(content);
	}

	if (!strcmp(nodename, "xdg_shell_server_side_deco.lab")) {
		rc.xdg_shell_server_side_deco = get_bool(content);
	} else if (!strcmp(nodename, "layout.keyboard.lab")) {
		setenv("XKB_DEFAULT_LAYOUT", content, 1);
	} else if (!strcmp(nodename, "name.theme")) {
		rc.theme_name = strdup(content);
	} else if (!strcmp(nodename, "name.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "size.font.theme")) {
		fill_font(nodename, content, font_place);
	}
}

static char *
nodename(xmlNode *node, char *buf, int len)
{
	if (!node || !node->name) {
		return NULL;
	}

	/* Ignore superflous 'text.' in node name */
	if (node->parent && !strcmp((char *)node->name, "text")) {
		node = node->parent;
	}

	char *p = buf;
	p[--len] = 0;
	for (;;) {
		const char *name = (char *)node->name;
		char c;
		while ((c = *name++) != 0) {
			*p++ = tolower(c);
			if (!--len)
				return buf;
		}
		*p = 0;
		node = node->parent;
		if (!node || !node->name) {
			return buf;
		}
		*p++ = '.';
		if (!--len) {
			return buf;
		}
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
		traverse(n);
	}
}

/* Exposed in header file to allow unit tests to parse buffers */
void
rcxml_parse_xml(struct buf *b)
{
	xmlDoc *d = xmlParseMemory(b->buf, b->len);
	if (!d) {
		warn("xmlParseMemory()");
		exit(EXIT_FAILURE);
	}
	xml_tree_walk(xmlDocGetRootElement(d));
	xmlFreeDoc(d);
	xmlCleanupParser();
}

static void
pre_processing(void)
{
	rc.xdg_shell_server_side_deco = true;
	rc.font_size_activewindow = 8;
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
	pre_processing();
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
set_title_height(void)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%s %d", rc.font_name_activewindow,
		 rc.font_size_activewindow);
	rc.title_height = font_height(buf);
}

static void
post_processing(void)
{
	if (!wl_list_length(&rc.keybinds)) {
		info("loading default key bindings");
		bind("A-Escape", "Exit", NULL);
		bind("A-Tab", "NextWindow", NULL);
		bind("A-F2", "NextWindow", NULL);
		bind("A-F3", "Execute", "dmenu_run");
	}

	if (!rc.theme_name) {
		rc.theme_name = strdup("Clearlooks");
	}
	if (!rc.font_name_activewindow) {
		rc.font_name_activewindow = strdup("sans");
	}
	set_title_height();
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
		warn("cannot find rc.xml config file");
		goto no_config;
	}

	/* Reading file into buffer before parsing - better for unit tests */
	stream = fopen(rcxml, "r");
	if (!stream) {
		warn("cannot read (%s)", rcxml);
		goto no_config;
	}
	info("reading config file (%s)", rcxml);
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

static void
free_safe(const void *p)
{
	if (p) {
		free((void *)p);
	}
	p = NULL;
}

void
rcxml_finish(void)
{
	free_safe(rc.font_name_activewindow);
	free_safe(rc.theme_name);

	struct keybind *k, *k_tmp;
	wl_list_for_each_safe (k, k_tmp, &rc.keybinds, link) {
		wl_list_remove(&k->link);
		free_safe(k->command);
		free_safe(k->action);
		free_safe(k->keysyms);
		free_safe(k);
	}
}

void
rcxml_get_nodenames(struct buf *b)
{
	write_to_nodename_buffer = true;
	nodename_buffer = b;
}
