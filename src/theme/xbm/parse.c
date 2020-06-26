#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "buf.h"
#include "xbm.h"

static unsigned char defaultcolor[] = { 127, 127, 127, 255 };
static unsigned char background[] = { 255, 255, 255, 255 };

static uint32_t *pixmap;

static void add_pixel(int position, unsigned char *rbga)
{
	pixmap[position] = (rbga[3] << 24) | (rbga[0] << 16) | (rbga[1] << 8) |
			   rbga[0];
}

static void init_pixmap(int w, int h)
{
	static bool has_run;
	if (has_run)
		return;
	has_run = true;
	pixmap = (uint32_t *)calloc(w * h, sizeof(uint32_t));
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
			if (value & bit) {
				add_pixel(row * width + col, defaultcolor);
				printf(".");
			} else {
				add_pixel(row * width + col, background);
				printf(" ");
			}
		}
		++t;
		printf("\n");
	}
}

cairo_surface_t *xbm_create_bitmap(struct token *tokens)
{
	cairo_surface_t *g_surface;
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
	g_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (!g_surface) {
		fprintf(stderr, "no surface\n");
		return NULL;
	}
	unsigned char *surface_data = cairo_image_surface_get_data(g_surface);
	cairo_surface_flush(g_surface);
	memcpy(surface_data, pixmap, width * height * 4);
	free(pixmap);
	cairo_surface_mark_dirty(g_surface);
	return g_surface;
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
