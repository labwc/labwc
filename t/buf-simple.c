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

static void
test_buf_add_fmt(void **state)
{
	(void)state;

	struct buf s = BUF_INIT;

	buf_add(&s, "foo");
	buf_add_fmt(&s, " %s baz %d", "bar", 10);
	assert_string_equal(s.data, "foo bar baz 10");

	buf_reset(&s);
}

static void
test_buf_add_char(void **state)
{
	(void)state;

	const char long_string[] = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
	size_t len = strlen(long_string);

	/*
	 * Start off with a long string so that the allocated buffer is only
	 * just large enough to contain the string and the NULL termination.
	 */
	struct buf s = BUF_INIT;
	buf_add(&s, long_string);
	assert_int_equal(s.alloc, len + 1);

	/* Check that buf_add_char() allocates space for the new character */
	buf_add_char(&s, '+');
	assert_true(s.alloc >= (int)len + 2);
	assert_string_equal(s.data + s.len - 1, "+");

	buf_reset(&s);
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_expand_title),
		cmocka_unit_test(test_buf_add_fmt),
		cmocka_unit_test(test_buf_add_char),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
