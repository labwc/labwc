// SPDX-License-Identifier: GPL-2.0-only
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <cmocka.h>
#include <wlr/config.h>
#include "common/macros.h"
#include "common/xml.h"
#include "config/libinput.h"

static bool
parse(const char *xml, struct lab_accel_curve *curve)
{
	xmlDoc *doc = xmlReadMemory(xml, strlen(xml), "curve.xml", NULL, 0);
	assert_non_null(doc);
	xmlNode *root = xmlDocGetRootElement(doc);
	lab_xml_expand_dotted_attributes(root);
	bool valid = libinput_curve_parse(root, curve);
	xmlFreeDoc(doc);
	return valid;
}

static void
test_curve(void **state)
{
	struct lab_accel_curve curve = {0};
	assert_true(parse("<scrollCurve step='0.10' points='0 0.10 0.4'/>", &curve));
	assert_int_equal(curve.nr_points, 3);
	assert_true(curve.step == 0.1);
	assert_true(curve.points[1] == 0.1);
	assert_true(parse("<motionCurve><step> 1 </step>"
		"<points> 0\t1\n2 </points></motionCurve>", &curve));
	assert_true(curve.step == 1);
	assert_int_equal(curve.nr_points, 3);
	assert_true(curve.points[2] == 2);
	/* Replacement must not retain points from the previous longer curve. */
	assert_true(parse("<scrollCurve step='1' points='0 1'/>", &curve));
	assert_int_equal(curve.nr_points, 2);
	assert_true(curve.points[2] == 0);
}

static void
test_max_points(void **state)
{
	char xml[512] = "<scrollCurve step='1' points='";
	size_t len = strlen(xml);
	for (size_t i = 0; i < LAB_ACCEL_CURVE_MAX_POINTS; ++i) {
		len += snprintf(xml + len, sizeof(xml) - len, "%zu ", i);
	}
	snprintf(xml + len, sizeof(xml) - len, "'/>");
	struct lab_accel_curve curve = {0};
	assert_true(parse(xml, &curve));
	assert_int_equal(curve.nr_points, LAB_ACCEL_CURVE_MAX_POINTS);
	assert_true(curve.points[LAB_ACCEL_CURVE_MAX_POINTS - 1]
		== LAB_ACCEL_CURVE_MAX_POINTS - 1);
}

static void
test_invalid_curve(void **state)
{
	static const char *const cases[] = {
		"<scrollCurve/>",
		"<scrollCurve step='' points='0 1'/>",
		"<scrollCurve step='1' points=''/>",
		"<scrollCurve step='1' points='  '/>",
		"<scrollCurve step='0' points='0 1'/>",
		"<scrollCurve step='-1' points='0 1'/>",
		"<scrollCurve step='nan' points='0 1'/>",
		"<scrollCurve step='inf' points='0 1'/>",
		"<scrollCurve step='1e999' points='0 1'/>",
		"<scrollCurve step='1' points='0'/>",
		"<scrollCurve step='1' points='0 -1'/>",
		"<scrollCurve step='1' points='0 nan'/>",
		"<scrollCurve step='1' points='0 inf'/>",
		"<scrollCurve step='1' points='0 1e999'/>",
		"<scrollCurve step='1' points='0 0.100.10'/>",
		"<scrollCurve step='1'>0 1</scrollCurve>",
		"<scrollCurve step='1' points='0 1'><step>2</step></scrollCurve>",
		"<scrollCurve step='1' points='0 1' typo='2'/>",
		"<scrollCurve step='1' points='0 1'><points>0 2</points></scrollCurve>",
		"<scrollCurve step='1'><points><nested>0 1</nested></points></scrollCurve>",
		"<scrollCurve step='1' points='0 1 2 3 4 5 6 7 8 9 10 11 12 "
			"13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32'/>",
	};
	for (size_t i = 0; i < ARRAY_SIZE(cases); ++i) {
		struct lab_accel_curve curve = {
			.step = 42,
			.points = {0, 3},
			.nr_points = 2,
		};
		assert_false(parse(cases[i], &curve));
		/* Parsing failures must not partially replace a prior curve. */
		assert_true(curve.step == 42);
		assert_int_equal(curve.nr_points, 2);
		assert_true(curve.points[1] == 3);
	}
}

#if WLR_HAS_LIBINPUT_BACKEND
static void
test_native_config(void **state)
{
	struct libinput_category category = {0};
	for (int i = LIBINPUT_ACCEL_TYPE_FALLBACK; i <= LIBINPUT_ACCEL_TYPE_SCROLL; ++i) {
		assert_true(parse("<curve step='1' points='0 1'/>",
			&category.accel_curves[i]));
	}
	struct libinput_config_accel *config = libinput_custom_accel_create(&category);
	assert_non_null(config);
	libinput_config_accel_destroy(config);
	category.accel_curve_invalid = true;
	assert_null(libinput_custom_accel_create(&category));
	category.accel_curve_invalid = false;
	category.accel_curves[LIBINPUT_ACCEL_TYPE_SCROLL].step = 0;
	assert_null(libinput_custom_accel_create(&category));
	/* An opt-in with no curves uses libinput defaults. */
	memset(&category, 0, sizeof(category));
	config = libinput_custom_accel_create(&category);
	assert_non_null(config);
	libinput_config_accel_destroy(config);
}
#endif

int
main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_curve),
		cmocka_unit_test(test_max_points),
		cmocka_unit_test(test_invalid_curve),
#if WLR_HAS_LIBINPUT_BACKEND
		cmocka_unit_test(test_native_config),
#endif
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
