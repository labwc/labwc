#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rcxml.h"

struct rcxml rc = { 0 };

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <rc.xml file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	rcxml_init(&rc);
	rcxml_set_verbose();
	rcxml_read(argv[1]);
}
