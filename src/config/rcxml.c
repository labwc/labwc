#define _POSIX_C_SOURCE 200809L
#include <assert.h>
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

#include "common/dir.h"
#include "common/log.h"
#include "common/nodename.h"
#include "common/string-helpers.h"
#include "common/zfree.h"
#include "config/keybind.h"
#include "config/rcxml.h"

static bool in_keybind = false;
static bool is_attribute = false;
static struct keybind *current_keybind;

enum font_place {
	FONT_PLACE_UNKNOWN = 0,
	FONT_PLACE_ACTIVEWINDOW,
	FONT_PLACE_INACTIVEWINDOW,
	/* TODO: Add all places based on Openbox's rc.xml */
};

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
	/* We expect <keybind key=""> to come first */
	assert(current_keybind);
	if (!strcmp(nodename, "name.action")) {
		current_keybind->action = strdup(content);
	} else if (!strcmp(nodename, "command.action")) {
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
	string_truncate_at_pattern(nodename, ".openbox_config");

	if (getenv("LABWC_DEBUG_CONFIG_NODENAMES")) {
		if (is_attribute) {
			printf("@");
		}
		printf("%s: %s\n", nodename, content);
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
	} else if (!strcmp(nodename, "name.theme")) {
		rc.theme_name = strdup(content);
	} else if (!strcmp(nodename, "cornerradius.theme")) {
		rc.corner_radius = atoi(content);
	} else if (!strcmp(nodename, "name.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "size.font.theme")) {
		fill_font(nodename, content, font_place);
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
rcxml_init()
{
	static bool has_run;

	if (has_run) {
		return;
	}
	has_run = true;
	LIBXML_TEST_VERSION
	wl_list_init(&rc.keybinds);
	rc.xdg_shell_server_side_deco = true;
	rc.corner_radius = 8;
	rc.font_size_activewindow = 10;
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
post_processing(void)
{
	if (!wl_list_length(&rc.keybinds)) {
		info("load default key bindings");
		bind("A-Escape", "Exit", NULL);
		bind("A-Tab", "NextWindow", NULL);
		bind("A-F3", "Execute", "bemenu-run");
	}

	if (!rc.theme_name) {
		rc.theme_name = strdup("Clearlooks-3.4");
	}
	if (!rc.font_name_activewindow) {
		rc.font_name_activewindow = strdup("sans");
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
		info("cannot find rc.xml config file; using defaults");
		goto no_config;
	}

	/* Reading file into buffer before parsing - better for unit tests */
	stream = fopen(rcxml, "r");
	if (!stream) {
		warn("cannot read (%s)", rcxml);
		goto no_config;
	}
	info("read config file (%s)", rcxml);
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
	zfree(rc.theme_name);

	struct keybind *k, *k_tmp;
	wl_list_for_each_safe (k, k_tmp, &rc.keybinds, link) {
		wl_list_remove(&k->link);
		zfree(k->command);
		zfree(k->action);
		zfree(k->keysyms);
		zfree(k);
	}
}
