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

static uint32_t u32(unsigned char *rbga)
{
	return (rbga[3] << 24) | (rbga[0] << 16) | (rbga[1] << 8) | rbga[0];
}

static void process_bytes(struct pixmap *pixmap, struct token *tokens)
{
	pixmap->data = (uint32_t *)calloc(pixmap->width * pixmap->height,
					  sizeof(uint32_t));
	struct token *t = tokens;
	for (int row = 0; row < pixmap->height; row++) {
		int byte = 1;
		for (int col = 0; col < pixmap->width; col++) {
			if (col == byte * 8) {
				++byte;
				++t;
			}
			if (!t->type)
				return;
			if (t->type != TOKEN_INT)
				return;
			int bit = 1 << (col % 8);
			if (t->value & bit)
				pixmap->data[row * pixmap->width + col] =
					u32(defaultcolor);
		}
		++t;
	}
}

struct pixmap xbm_create_pixmap(struct token *tokens)
{
	struct pixmap pixmap = { 0 };

	for (struct token *t = tokens; t->type; t++) {
		if (pixmap.width && pixmap.height) {
			if (t->type != TOKEN_INT)
				continue;
			process_bytes(&pixmap, t);
			goto out;
		}
		if (strstr(t->name, "width"))
			pixmap.width = atoi((++t)->name);
		else if (strstr(t->name, "height"))
			pixmap.height = atoi((++t)->name);
	}
out:
	return pixmap;
}

/* Assuming a 6x6 button for the time being */
/* TODO: pass width, height, vargs bytes */
struct pixmap xbm_create_pixmap_builtin(const char *button)
{
	struct pixmap pixmap = { 0 };

	pixmap.width = 6;
	pixmap.height = 6;

	struct token t[7];
	for (int i = 0; i < 6; i++) {
		t[i].value = button[i];
		t[i].type = TOKEN_INT;
	}
	t[6].type = 0;
	process_bytes(&pixmap, t);
	return pixmap;
}

char *xbm_read_file(const char *filename)
{
	char *line = NULL;
	size_t len = 0;
	FILE *stream = fopen(filename, "r");
	if (!stream)
		return NULL;
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
