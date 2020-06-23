#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "xbm.h"

static void process_bytes(int height, int width, struct token *tokens)
{
	struct token *t = tokens;
	for (int row = 0; row < height; row++) {
		int byte = 1;
		for (int col = 0; col < width; col++) {
			if (col == byte * 8) {
				++byte;
				++t;
			}
			if (!t->type)
				return;
			int value = (int)strtol(t->name, NULL, 0);
			int bit = 1 << (col % 8);
			if (value & bit)
				printf(".");
			else
				printf(" ");
		}
		++t;
		printf("\n");
	}
}

void xbm_create_bitmap(struct token *tokens)
{
	int width = 0, height = 0;
	for (struct token *t = tokens; t->type; t++) {
		if (width && height) {
			if (t->type != TOKEN_INT)
				continue;
			process_bytes(width, height, t);
			return;
		}
		if (strstr(t->name, "width"))
			width = atoi((++t)->name);
		else if (strstr(t->name, "height"))
			height = atoi((++t)->name);
	}
}

char *xbm_read_file(const char *filename)
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
