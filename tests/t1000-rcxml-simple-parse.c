#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

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

int main(int argc, char **argv)
{
	plan(1);

	char template[] = "temp_file_XXXXXX";
	int fd = mkstemp(template);
	if (fd < 0)
		exit(1);
	write(fd, src, sizeof(src) - 1);

	rcxml_read(template);
	unlink(template);

	diag("Simple parse rc.xml");
	ok1(rc.client_side_decorations);

	rcxml_finish();
	return exit_status();
}
