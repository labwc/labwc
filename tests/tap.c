#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>

#include "tap.h"

static int nr_tests_run;
static int nr_tests_expected;
static int nr_tests_failed;

void plan(int nr_tests)
{
	static bool run_once;

	if (run_once)
		return;
	run_once = true;
	printf("1..%d\n", nr_tests);
	nr_tests_expected = nr_tests;
}

void diag(const char *fmt, ...)
{
	va_list params;

	fprintf(stdout, "# ");
	va_start(params, fmt);
	vfprintf(stdout, fmt, params);
	va_end(params);
	fprintf(stdout, "\n");
}

int ok(int result, const char *testname, ...)
{
	va_list params;

	++nr_tests_run;
	if (!result) {
		printf("not ");
		nr_tests_failed++;
	}
	printf("ok %d", nr_tests_run);
	if (testname) {
		printf(" - ");
		va_start(params, testname);
		vfprintf(stdout, testname, params);
		va_end(params);
	}
	printf("\n");
	if (!result)
		diag("    Failed test");
	return result ? 1 : 0;
}

int exit_status(void)
{
	int ret;

	if (nr_tests_expected != nr_tests_run) {
		diag("expected=%d; run=%d; failed=%d", nr_tests_expected,
		     nr_tests_run, nr_tests_failed);
	}
	if (nr_tests_expected < nr_tests_run)
		ret = nr_tests_run - nr_tests_expected;
	else
		ret = nr_tests_failed + nr_tests_expected - nr_tests_run;
	if (ret > 255)
		ret = 255;
	return ret;
}
