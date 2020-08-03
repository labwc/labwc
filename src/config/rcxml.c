#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>
#include <fcntl.h>
#include <wayland-server-core.h>

#include "config/rcxml.h"
#include "config/keybind.h"
#include "config/config-dir.h"
#include "common/bug-on.h"

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

static void rstrip(char *buf, const char *pattern)
{
	char *p = strstr(buf, pattern);
	if (!p)
		return;
	*p = '\0';
}

static void fill_keybind(char *nodename, char *content)
{
	if (!content)
		return;
	rstrip(nodename, ".keybind.keyboard");
	if (!strcmp(nodename, "key")) {
		current_keybind = keybind_add(content);
		fprintf(stderr, "[bind] %s: ", content);
	}
	/* We expect <keybind key=""> to come first */
	BUG_ON(!current_keybind);
	if (!strcmp(nodename, "name.action")) {
		current_keybind->action = strdup(content);
		fprintf(stderr, "%s", content);
	} else if (!strcmp(nodename, "command.action")) {
		current_keybind->command = strdup(content);
		fprintf(stderr, " - %s", content);
	}
}

static bool get_bool(const char *s)
{
	if (!s)
		return false;
	if (!strcasecmp(s, "yes"))
		return true;
	if (!strcasecmp(s, "true"))
		return true;
	return false;
}

static void fill_font(char *nodename, char *content, enum font_place place)
{
	if (!content)
		return;
	rstrip(nodename, ".font.theme");

	/* TODO: implement for all font places */
	if (place != FONT_PLACE_ACTIVEWINDOW)
		return;
	if (!strcmp(nodename, "name"))
		rc.font_name_activewindow = strdup(content);
	else if (!strcmp(nodename, "size"))
		rc.font_size_activewindow = atoi(content);
}

static enum font_place enum_font_place(const char *place)
{
	if (!place)
		return FONT_PLACE_UNKNOWN;
	if (!strcasecmp(place, "ActiveWindow"))
		return FONT_PLACE_ACTIVEWINDOW;
	else if (!strcasecmp(place, "InactiveWindow"))
		return FONT_PLACE_INACTIVEWINDOW;
	return FONT_PLACE_UNKNOWN;
}

static void entry(xmlNode *node, char *nodename, char *content)
{
	/* current <theme><font place=""></theme> */
	static enum font_place font_place = FONT_PLACE_UNKNOWN;

	if (!nodename)
		return;
	rstrip(nodename, ".openbox_config");

	/* for debugging */
	if (write_to_nodename_buffer) {
		if (is_attribute)
			buf_add(nodename_buffer, "@");
		buf_add(nodename_buffer, nodename);
		if (content) {
			buf_add(nodename_buffer, ": ");
			buf_add(nodename_buffer, content);
		}
		buf_add(nodename_buffer, "\n");
	}

	if (!content)
		return;
	if (in_keybind)
		fill_keybind(nodename, content);

	if (is_attribute && !strcmp(nodename, "place.font.theme"))
		font_place = enum_font_place(content);

	if (!strcmp(nodename, "csd.lab"))
		rc.client_side_decorations = get_bool(content);
	else if (!strcmp(nodename, "layout.keyboard.lab"))
		setenv("XKB_DEFAULT_LAYOUT", content, 1);
	else if (!strcmp(nodename, "name.theme"))
		rc.theme_name = strdup(content);
	else if (!strcmp(nodename, "name.font.theme"))
		fill_font(nodename, content, font_place);
	else if (!strcmp(nodename, "size.font.theme"))
		fill_font(nodename, content, font_place);
}

static char *nodename(xmlNode *node, char *buf, int len)
{
	if (!node || !node->name)
		return NULL;

	/* Ignore superflous 'text.' in node name */
	if (node->parent && !strcmp((char *)node->name, "text"))
		node = node->parent;

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
		if (!node || !node->name)
			return buf;
		*p++ = '.';
		if (!--len)
			return buf;
	}
}

static void process_node(xmlNode *node)
{
	char *content;
	static char buffer[256];
	char *name;

	content = (char *)node->content;
	if (xmlIsBlankNode(node))
		return;
	name = nodename(node, buffer, sizeof(buffer));
	entry(node, name, content);
}

static void xml_tree_walk(xmlNode *node);

static void traverse(xmlNode *n)
{
	process_node(n);
	is_attribute = true;
	for (xmlAttr *attr = n->properties; attr; attr = attr->next)
		xml_tree_walk(attr->children);
	is_attribute = false;
	xml_tree_walk(n->children);
}

static void xml_tree_walk(xmlNode *node)
{
	for (xmlNode *n = node; n && n->name; n = n->next) {
		if (!strcasecmp((char *)n->name, "comment"))
			continue;
		if (!strcasecmp((char *)n->name, "keybind")) {
			in_keybind = true;
			traverse(n);
			in_keybind = false;
			fprintf(stderr, "\n");
			continue;
		}
		traverse(n);
	}
}

/* Exposed in header file to allow unit tests to parse buffers */
void rcxml_parse_xml(struct buf *b)
{
	xmlDoc *d = xmlParseMemory(b->buf, b->len);
	if (!d) {
		fprintf(stderr, "fatal: xmlParseMemory()\n");
		exit(EXIT_FAILURE);
	}
	xml_tree_walk(xmlDocGetRootElement(d));
	xmlFreeDoc(d);
	xmlCleanupParser();
}

static void rcxml_init()
{
	LIBXML_TEST_VERSION
}

static void bind(const char *binding, const char *action)
{
	if (!binding || !action)
		return;
	struct keybind *k = keybind_add(binding);
	if (k)
		k->action = strdup(action);
	fprintf(stderr, "binding: %s: %s\n", binding, action);
}

static void post_processing(void)
{
	if (!wl_list_length(&rc.keybinds)) {
		fprintf(stderr, "info: loading default key bindings\n");
		bind("A-Escape", "Exit");
		bind("A-Tab", "NextWindow");
		bind("A-F3", "Execute");
	}
	/* TODO: Set all char* variables if NULL */
}

static void rcxml_path(char *buf, size_t len, const char *filename)
{
	if (filename)
		snprintf(buf, len, "%s", filename);
	else
		snprintf(buf, len, "%s/rc.xml", config_dir());
}

void rcxml_read(const char *filename)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	struct buf b;
	char rcxml[4096];

	rcxml_init();
	wl_list_init(&rc.keybinds);

	/*
	 * Reading file into buffer before parsing makes it easier to write
	 * unit tests.
	 */
	rcxml_path(rcxml, sizeof(rcxml), filename);
	fprintf(stderr, "info: read config file (%s)\n", rcxml);
	stream = fopen(rcxml, "r");
	if (!stream) {
		fprintf(stderr, "warn: cannot read '%s'\n", rcxml);
		goto out;
	}
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
out:
	post_processing();
}

void rcxml_get_nodenames(struct buf *b)
{
	write_to_nodename_buffer = true;
	nodename_buffer = b;
}
