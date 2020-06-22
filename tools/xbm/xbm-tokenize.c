#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "xbm.h"

/* Read file into buffer, because it's easier to tokenize that way */
char *read_file(const char *filename)
{
	char *line = NULL;
	size_t len = 0;
	FILE *stream = fopen(filename, "r");
	if (!stream) {
		fprintf(stderr, "warn: cannot read '%s'\n", filename);
		return NULL;
	}
	struct buf buffer;
	buf_init(&buffer);
	while ((getline(&line, &len, stream) != -1)) {
		char *p = strrchr(line, '\n');
		if (p)
			*p = '\0';
		buf_add(&buffer, line);
	}
	free(line);
	fclose(stream);
	return (buffer.buf);
}

int main(int argc, char **argv)
{
	struct token *tokens;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <xbm-file>\n", argv[0]);
		return 1;
	}

	char *buffer = read_file(argv[1]);
	if (!buffer)
		exit(EXIT_FAILURE);
	tokens = tokenize(buffer);
	free(buffer);
	for (struct token *t = tokens; t->type; t++)
		printf("%s\n", t->name);
}
