#include <stdio.h>
#include <stdlib.h>

#include "xbm.h"

int main(int argc, char **argv)
{
	struct token *tokens;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <xbm-file>\n", argv[0]);
		return 1;
	}

	char *buffer = xbm_read_file(argv[1]);
	if (!buffer)
		exit(EXIT_FAILURE);
	tokens = xbm_tokenize(buffer);
	free(buffer);

	xbm_create_bitmap(tokens);
	free(tokens);
}
