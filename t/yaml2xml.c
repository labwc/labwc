// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>
#include "common/buf.h"
#include "common/macros.h"
#include "common/yaml2xml.h"

struct test_set {
	const char *name, *yaml, *xml;
};

const struct test_set test_sets[] = {
	{
		.name = "key-scalar",
		.yaml = "xxx: yyy",
		.xml = "<test><xxx>yyy</xxx></test>",
	},
	{
		.name = "key-sequence",
		.yaml = "xxx: [yyy, zzz]",
		.xml = "<test><xxx>yyy</xxx><xxx>zzz</xxx></test>",
	},
	{
		.name = "key-mapping",
		.yaml = "xxx: {yyy: zzz}",
		.xml = "<test><xxx><yyy>zzz</yyy></xxx></test>",
	},
	{
		.name = "window-switcher-fields",
		.yaml = "windowSwitcher: {fields: [xxx, yyy]}",
		.xml =
			"<test><windowSwitcher><fields>"
				"<field>xxx</field>"
				"<field>yyy</field>"
			"</fields></windowSwitcher></test>",
	},
	{
		.name = "theme-fonts",
		.yaml = "theme: {fonts: [xxx, yyy]}",
		.xml =
			"<test><theme>"
				"<font>xxx</font>"
				"<font>yyy</font>"
			"</theme></test>",
	},
	{
		.name = "mousebinds",
		.yaml =
			"mousebinds:\n"
        		"  - { button: W-Left,  action: Press, actions: [ { name: Raise }, { name: Move } ] }\n"
			"  - { button: W-Right, action: Drag,  action: { name: Resize} }\n",
		.xml =
			"<test>"
			"<mousebind>"
				"<button>W-Left</button>"
				"<action>Press</action>"
				"<action><name>Raise</name></action>"
				"<action><name>Move</name></action>"
			"</mousebind>"
			"<mousebind>"
				"<button>W-Right</button>"
				"<action>Drag</action>"
				"<action><name>Resize</name></action>"
			"</mousebind>"
			"</test>",
	},
};

static void
test_yaml_to_xml(void **state)
{
	(void)state;
	for (int i = 0; i < (int)ARRAY_SIZE(test_sets); i++) {
		const struct test_set *set = &test_sets[i];

		char buf[1024];
		FILE *stream = fmemopen(buf, sizeof(buf), "w+");
		fwrite(set->yaml, strlen(set->yaml), 1, stream);
		fseek(stream, 0, SEEK_SET);

		struct buf b = yaml_to_xml(stream, "test");
		fclose(stream);
		assert_string_equal(b.data, set->xml);
	}
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_yaml_to_xml),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
