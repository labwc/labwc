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

#include "rcxml.h"

static bool in_keybind = false;
static bool is_attribute = false;
static bool verbose = false;

static void rstrip(char *buf, const char *pattern)
{
	char *p = strstr(buf, pattern);
	if (!p)
		return;
	*p = '\0';
}

static void fill_keybind(xmlNode *n, char *nodename, char *content)
{
	rstrip(nodename, ".keybind.keyboard");
	if (!strcmp(nodename, "name.action")) {
		; /* TODO: populate keybind with stuff */
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

static void entry(xmlNode *node, char *nodename, char *content)
{
	if (!nodename)
		return;
	rstrip(nodename, ".openbox_config");
	if (verbose) {
		if (is_attribute)
			printf("@");
		printf("%s: %s\n", nodename, content);
	}
	if (!content)
		return;
	if (in_keybind)
		fill_keybind(node, nodename, content);
	if (!strcmp(nodename, "csd.lab"))
		rc.client_side_decorations = get_bool(content);
	if (!strcmp(nodename, "layout.keyboard.lab"))
		setenv("XKB_DEFAULT_LAYOUT", content, 1);
}

static void keybind_begin(void)
{
	/* TODO: xcalloc struct keybind */
	in_keybind = true;
}

static void keybind_end(void)
{
	in_keybind = false;
	/* TODO: wl_list_add keybind */
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
			keybind_begin();
			traverse(n);
			keybind_end();
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

void rcxml_init(struct rcxml *rc)
{
	LIBXML_TEST_VERSION
}

void rcxml_read(const char *filename)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	ssize_t n_read;
	struct buf b;

	/* Read <filename> into buffer and then call rcxml_parse_xml() */
	stream = fopen(filename, "r");
	if (!stream) {
		fprintf(stderr, "warn: cannot read '%s'\n", filename);
		return;
	}
	buf_init(&b);
	while ((n_read = getline(&line, &len, stream) != -1)) {
		char *p = strrchr(line, '\n');
		if (p)
			*p = '\0';
		buf_add(&b, line);
	}
	free(line);
	fclose(stream);
	rcxml_parse_xml(&b);
	free(b.buf);
}

void rcxml_set_verbose(void)
{
	verbose = true;
}
