#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>

#include "theme/xbm/parse.h"

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
	struct pixmap pixmap = xbm_create_pixmap(tokens);
	free(tokens);

	cairo_surface_t *g_surface;
	g_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					       pixmap.width, pixmap.height);
	if (!g_surface) {
		fprintf(stderr, "no surface\n");
		exit(EXIT_FAILURE);
	}
	unsigned char *surface_data = cairo_image_surface_get_data(g_surface);
	cairo_surface_flush(g_surface);
	memcpy(surface_data, pixmap.data, pixmap.width * pixmap.height * 4);
	if (pixmap.data)
		free(pixmap.data);
	cairo_surface_mark_dirty(g_surface);

	char png_name[1024];
	snprintf(png_name, sizeof(png_name), "%s.png", argv[1]);
	if (cairo_surface_write_to_png(g_surface, png_name)) {
		fprintf(stderr, "cannot save png\n");
		exit(EXIT_FAILURE);
	}
	cairo_surface_destroy(g_surface);
	exit(EXIT_SUCCESS);
}
