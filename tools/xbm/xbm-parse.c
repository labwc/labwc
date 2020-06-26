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

	cairo_surface_t *surface = xbm_create_bitmap(tokens);
	free(tokens);

	if (!surface)
		fprintf(stderr, "no surface\n");
	char png_name[1024];
	snprintf(png_name, sizeof(png_name), "%s.png", argv[1]);
	if (cairo_surface_write_to_png(surface, png_name)) {
		fprintf(stderr, "cannot save png\n");
		exit(EXIT_FAILURE);
	}
	cairo_surface_destroy(surface);
	exit(EXIT_SUCCESS);
}
