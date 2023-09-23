// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>
#include "common/buf.h"
#include "common/mem.h"

static void
test_expand_title(void **state)
{
	(void)state;

	struct buf s = BUF_INIT;

	char TEMPLATE[] = "foo ~/bar";
	char expect[4096];
	snprintf(expect, sizeof(expect), "foo %s/bar", getenv("HOME"));

	buf_add(&s, TEMPLATE);
	assert_string_equal(s.data, TEMPLATE);
	assert_int_equal(s.len, strlen(TEMPLATE));

	// Resolve ~
	buf_expand_tilde(&s);
	assert_string_equal(s.data, expect);
	assert_int_equal(s.len, strlen(expect));

	setenv("bar", "BAR", 1);

	// Resolve $bar and ${bar}
	s.len = 0;
	buf_add(&s, "foo $bar baz");
	buf_expand_shell_variables(&s);
	assert_string_equal(s.data, "foo BAR baz");
	assert_int_equal(s.len, 11);

	s.len = 0;
	buf_add(&s, "foo ${bar} baz");
	buf_expand_shell_variables(&s);
	assert_string_equal(s.data, "foo BAR baz");
	assert_int_equal(s.len, 11);

	// Don't resolve $()
	s.len = 0;
	buf_add(&s, "foo $(bar) baz");
	buf_expand_shell_variables(&s);
	assert_string_equal(s.data, "foo $(bar) baz");
	assert_int_equal(s.len, 14);

	unsetenv("bar");
	free(s.data);
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_expand_title),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
