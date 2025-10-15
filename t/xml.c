// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>
#include "common/macros.h"
#include "common/xml.h"

struct test_case {
	const char *before, *after;
} test_cases[] = {{
	"<keybind name.action='ShowMenu' menu.action='root-menu' "
		"x.position.action='1' y.position.action='2'/>",

	"<keybind>"
		"<action>"
			"<name>ShowMenu</name>"
			"<menu>root-menu</menu>"
			"<position>"
				"<x>1</x>"
				"<y>2</y>"
			"</position>"
		"</action>"
	"</keybind>"
}, {
	"<AAA aaa='111' bbb='222'/>",

	"<AAA>"
		"<aaa>111</aaa>"
		"<bbb>222</bbb>"
	"</AAA>"
}, {
	"<AAA aaa.bbb.ccc='111' ddd.ccc='222' eee.bbb.ccc='333'/>",

	"<AAA><ccc>"
		"<bbb><aaa>111</aaa></bbb>"
		"<ddd>222</ddd>"
		"<bbb><eee>333</eee></bbb>"
	"</ccc></AAA>"
}, {
	"<AAA aaa.bbb.ccc='111' bbb.ccc='222' ddd.bbb.ccc='333'/>",

	"<AAA><ccc><bbb>"
		"<aaa>111</aaa>"
		"222"
		"<ddd>333</ddd>"
	"</bbb></ccc></AAA>"
}, {
	"<AAA aaa.bbb='111' aaa.ddd='222'/>",

	"<AAA>"
		"<bbb><aaa>111</aaa></bbb>"
		"<ddd><aaa>222</aaa></ddd>"
	"</AAA>"
}, {
	"<AAA aaa.bbb='111' bbb='222' ccc.bbb='333'/>",

	"<AAA><bbb>"
		"<aaa>111</aaa>"
		"222"
		"<ccc>333</ccc>"
	"</bbb></AAA>",
}, {
	"<AAA>"
		"<BBB aaa.bbb='111'/>"
		"<BBB aaa.bbb='111'/>"
	"</AAA>",

	"<AAA>"
		"<BBB><bbb><aaa>111</aaa></bbb></BBB>"
		"<BBB><bbb><aaa>111</aaa></bbb></BBB>"
	"</AAA>",
}, {
	"<AAA bbb.ccc='111'>"
		"<BBB>222</BBB>"
	"</AAA>",

	"<AAA>"
		"<ccc><bbb>111</bbb></ccc>"
		"<BBB>222</BBB>"
	"</AAA>",
}, {
	"<AAA>"
		"<BBB><CCC>111</CCC></BBB>"
		"<BBB><CCC>111</CCC></BBB>"
	"</AAA>",

	"<AAA>"
		"<BBB><CCC>111</CCC></BBB>"
		"<BBB><CCC>111</CCC></BBB>"
	"</AAA>",
}, {
	"<AAA aaa..bbb.ccc.='111' />",

	"<AAA><ccc><bbb><aaa>111</aaa></bbb></ccc></AAA>"
}};

static void
test_lab_xml_expand_dotted_attributes(void **state)
{
	for (size_t i = 0; i < ARRAY_SIZE(test_cases); i++) {
		xmlDoc *doc = xmlReadDoc((xmlChar *)test_cases[i].before,
					NULL, NULL, 0);
		xmlNode *root = xmlDocGetRootElement(doc);

		lab_xml_expand_dotted_attributes(root);

		xmlBuffer *buf = xmlBufferCreate();
		xmlNodeDump(buf, root->doc, root, 0, 0);
		assert_string_equal(test_cases[i].after, (char *)xmlBufferContent(buf));
		xmlBufferFree(buf);

		xmlFreeDoc(doc);
	}
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lab_xml_expand_dotted_attributes),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
