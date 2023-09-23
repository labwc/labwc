// SPDX-License-Identifier: GPL-2.0-only
#include "tap.h"
#include "common/buf.h"
#include "common/mem.h"

int main(int argc, char **argv)
{
	plan(10);

	struct buf s;
	buf_init(&s);

	diag("buf: expand tilde");

	char TEMPLATE[] = "foo ~/bar";
	char expect[4096];
	snprintf(expect, sizeof(expect), "foo %s/bar", getenv("HOME"));

	buf_add(&s, TEMPLATE);
	ok1(!strcmp(s.buf, TEMPLATE));
	ok1(s.len == strlen(TEMPLATE));

	// Resolve ~
	buf_expand_tilde(&s);
	ok1(!strcmp(s.buf, expect));
	ok1(s.len == strlen(expect));

	diag("buf: expand environment variable");
	setenv("bar", "BAR", 1);

	// Resolve $bar and ${bar}
	s.len = 0;
	buf_add(&s, "foo $bar baz");
	buf_expand_shell_variables(&s);
	ok1(!strcmp(s.buf, "foo BAR baz"));
	ok1(s.len == 11);

	s.len = 0;
	buf_add(&s, "foo ${bar} baz");
	buf_expand_shell_variables(&s);
	ok1(!strcmp(s.buf, "foo BAR baz"));
	ok1(s.len == 11);

	// Don't resolve $()
	s.len = 0;
	buf_add(&s, "foo $(bar) baz");
	buf_expand_shell_variables(&s);
	ok1(!strcmp(s.buf, "foo $(bar) baz"));
	ok1(s.len == 14);

	unsetenv("bar");
	free(s.buf);

	return exit_status();
}
