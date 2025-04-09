// SPDX-License-Identifier: GPL-2.0-only
#include <glib.h>
#include <stdbool.h>
#include <strings.h>
#include "common/xml.h"

/*
 * Converts a property A.B.C="X" into <C><B><A>X</A></B></C>
 */
static xmlNode*
create_prop_tree(const xmlAttr *prop)
{
	if (!strchr((char *)prop->name, '.')) {
		return NULL;
	}

	gchar **parts = g_strsplit((char *)prop->name, ".", -1);
	int length = g_strv_length(parts);
	xmlNode *root_node = NULL;
	xmlNode *parent_node = NULL;
	xmlNode *current_node = NULL;

	for (int i = length - 1; i >= 0; i--) {
		gchar *part = parts[i];
		if (!*part) {
			/* Ignore empty string */
			continue;
		}
		current_node = xmlNewNode(NULL, (xmlChar *)part);
		if (parent_node) {
			xmlAddChild(parent_node, current_node);
		} else {
			root_node = current_node;
		}
		parent_node = current_node;
	}

	xmlChar *content = xmlNodeGetContent(prop->children);
	xmlNodeSetContent(current_node, content);
	xmlFree(content);

	g_strfreev(parts);
	return root_node;
}

/*
 * Consider <keybind name.action="ShowMenu" x.position.action="1" y.position="2" />.
 * These three properties are represented by following trees.
 *    action(dst)---name
 *    action(src)---position---x
 *    action--------position---y
 * When we call merge_two_trees(dst, src), we walk over the trees from their
 * roots towards leaves, and merge the identical node 'action' like:
 *    action(dst)---name
 *         \--------position---x
 *    action(src)---position---y
 * And when we call merge_two_trees(dst, src) again, we walk over the dst tree
 * like 'action'->'position'->'x' and the src tree like 'action'->'position'->'y'.
 * First, we merge the identical node 'action' again like:
 *    action---name
 *         \---position(dst)---x
 *          \--position(src)---y
 * Next, we merge the identical node 'position' like:
 *    action---name
 *         \---position---x
 *                   \----y
 */
static bool
merge_two_trees(xmlNode *dst, xmlNode *src)
{
	bool merged = false;

	while (dst && src && src->children
			&& !strcasecmp((char *)dst->name, (char *)src->name)) {
		xmlNode *next_dst = dst->last;
		xmlNode *next_src = src->children;
		xmlAddChild(dst, src->children);
		xmlUnlinkNode(src);
		xmlFreeNode(src);
		src = next_src;
		dst = next_dst;
		merged = true;
	}

	return merged;
}

void
lab_xml_expand_dotted_props(xmlNode *parent)
{
	xmlNode *old_first_child = parent->children;
	xmlNode *prev_tree = NULL;

	if (parent->type != XML_ELEMENT_NODE) {
		return;
	}

	for (xmlAttr *prop = parent->properties; prop;) {
		/* Convert the property with dots into an xml tree */
		xmlNode *tree = create_prop_tree(prop);
		if (!tree) {
			/* The property doesn't contain dots */
			prev_tree = NULL;
			prop = prop->next;
			continue;
		}

		/* Try to merge the tree with the previous one */
		if (!merge_two_trees(prev_tree, tree)) {
			/* If not merged, add the tree as a new child */
			if (old_first_child) {
				xmlAddPrevSibling(old_first_child, tree);
			} else {
				xmlAddChild(parent, tree);
			}
			prev_tree = tree;
		}

		xmlAttr *next_prop = prop->next;
		xmlRemoveProp(prop);
		prop = next_prop;
	}

	for (xmlNode *node = parent->children; node; node = node->next) {
		lab_xml_expand_dotted_props(node);
	}
}
