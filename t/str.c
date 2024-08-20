// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>
#include "common/string-helpers.h"

static void
test_str_starts_with(void **state)
{
	(void)state;

	assert_true(str_starts_with("  foo", 'f', " \t\r\n"));
	assert_true(str_starts_with("f", 'f', " \t\r\n"));
	assert_false(str_starts_with("  foo", '<', " "));
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_str_starts_with),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
