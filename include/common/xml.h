/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_XML_H
#define LABWC_XML_H

#include <libxml/tree.h>

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

#endif /* LABWC_XML_H */
