/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_XML_H
#define LABWC_XML_H

#include <libxml/tree.h>
#include <stdbool.h>

/*
 * Converts dotted attributes into nested nodes.
 * For example, the following node:
 *
 *   <keybind name.action="ShowMenu" menu.action="root-menu"
 *            x.position.action="1" y.position.action="2" />
 *
 * is converted to:
 *
 *   <keybind>
 *     <action>
 *       <name>ShowMenu</name>
 *       <menu>root-menu</menu>
 *       <position>
 *         <x>1</x>
 *         <y>2</y>
 *       </position>
 *     </action>
 *   </keybind>
 */
void lab_xml_expand_dotted_attributes(xmlNode *root);

/* Returns true if the node only contains a string or is empty */
bool lab_xml_node_is_leaf(xmlNode *node);

bool lab_xml_get_node(xmlNode *node, const char *key, xmlNode **dst_node);
bool lab_xml_get_string(xmlNode *node, const char *key, char *s, size_t len);
bool lab_xml_get_int(xmlNode *node, const char *key, int *i);
bool lab_xml_get_bool(xmlNode *node, const char *key, bool *b);

/* also skips other unusual nodes like comments */
static inline xmlNode *
lab_xml_skip_text(xmlNode *child)
{
	while (child && child->type != XML_ELEMENT_NODE) {
		child = child->next;
	}
	return child;
}

static inline void
lab_xml_get_key_and_content(xmlNode *node, char **name, char **content)
{
	if (node) {
		*name = (char *)node->name;
		*content = (char *)xmlNodeGetContent(node);
	}
}

#define LAB_XML_FOR_EACH(parent, child, key, content) \
	for ((child) = lab_xml_skip_text((parent)->children), \
		lab_xml_get_key_and_content((child), &(key), &(content)); \
		\
		(child); \
		\
		xmlFree((xmlChar *)(content)), \
		(child) = lab_xml_skip_text((child)->next), \
		lab_xml_get_key_and_content((child), &(key), &(content)))

#endif /* LABWC_XML_H */
