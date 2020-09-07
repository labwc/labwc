#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#include "config/rcxml.h"
#include "tap.h"

struct rcxml rc = { 0 };

static char src[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<openbox_config>\n"
"<lab>\n"
"  <csd>yes</csd>\n"
"</lab>\n"
"</openbox_config>\n";

static char expect[] =
"openbox_config\n"
"lab\n"
"csd.lab\n"
"csd.lab: yes\n";

int main(int argc, char **argv)
{
	struct buf actual, source;

	buf_init(&actual);
	buf_init(&source);
	buf_add(&source, src);

	plan(1);
	diag("Parse simple rc.xml and read nodenames");

	rcxml_get_nodenames(&actual);
	rcxml_parse_xml(&source);
	printf("%s\n", actual.buf);
	printf("%s\n", expect);

	ok1(!strcmp(expect, actual.buf));
	free(actual.buf);
	free(source.buf);
	pango_cairo_font_map_set_default(NULL);
	return exit_status();
}
