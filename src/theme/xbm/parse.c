/*
 * Parse xbm token to create pixmap
 *
 * Copyright Johan Malm 2020
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "buf.h"
#include "theme/xbm/parse.h"

/* TODO: should be window.active.button.unpressed.image.color */
static unsigned char defaultcolor[] = { 255, 255, 255, 255 };

static uint32_t *data;

static void add_pixel(int position, unsigned char *rbga)
{
	data[position] = (rbga[3] << 24) | (rbga[0] << 16) | (rbga[1] << 8) |
			 rbga[0];
}

static void init_pixmap(int w, int h)
{
	static bool has_run;
	if (has_run)
		return;
	has_run = true;
	data = (uint32_t *)calloc(w * h, sizeof(uint32_t));
}

static void process_bytes(int height, int width, struct token *tokens)
{
	init_pixmap(width, height);
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
				add_pixel(row * width + col, defaultcolor);
		}
		++t;
	}
}

struct pixmap xbm_create_pixmap(struct token *tokens)
{
	struct pixmap pixmap;
	int width = 0, height = 0;
	for (struct token *t = tokens; t->type; t++) {
		if (width && height) {
			if (t->type != TOKEN_INT)
				continue;
			process_bytes(width, height, t);
			goto out;
		}
		if (strstr(t->name, "width"))
			width = atoi((++t)->name);
		else if (strstr(t->name, "height"))
			height = atoi((++t)->name);
	}
out:
	pixmap.data = data;
	pixmap.width = width;
	pixmap.height = height;
	return pixmap;
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
