#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#include "config/rcxml.h"
#include "common/buf.h"
#include "common/log.h"

struct rcxml rc = { 0 };

int main(int argc, char **argv)
{
	struct buf b;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <rc.xml file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	buf_init(&b);
	rcxml_get_nodenames(&b);
	rcxml_read(argv[1]);
	printf("%s", b.buf);
	free(b.buf);
	pango_cairo_font_map_set_default(NULL);
}
