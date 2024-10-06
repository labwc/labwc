// SPDX-License-Identifier: LGPL-2.0-or-later
/*
 * XPM image loader adapted from gdk-pixbuf
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * Adapted for labwc by John Lindgren, 2024
 */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>

#include "buffer.h"
#include "common/buf.h"
#include "common/macros.h"
#include "common/mem.h"
#include "img/img-xpm.h"

#include "xpm-color-table.h"

enum buf_op { op_header, op_cmap, op_body };

struct xpm_color {
	char *color_string;
	uint32_t argb;
};

struct file_handle {
	FILE *infile;
	struct buf buf;
};

static inline uint32_t
make_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static int
compare_xcolor_entries(const void *a, const void *b)
{
	return g_ascii_strcasecmp((const char *)a,
		color_names + ((const struct xcolor_entry *)b)->name_offset);
}

static bool
lookup_named_color(const char *name, uint32_t *argb)
{
	struct xcolor_entry *found = bsearch(name, xcolors, ARRAY_SIZE(xcolors),
		sizeof(struct xcolor_entry), compare_xcolor_entries);
	if (!found) {
		return false;
	}

	*argb = make_argb(0xFF, found->red, found->green, found->blue);
	return true;
}

static bool
parse_color(const char *spec, uint32_t *argb)
{
	if (spec[0] != '#') {
		return lookup_named_color(spec, argb);
	}

	int red, green, blue;
	switch (strlen(spec + 1)) {
	case 3:
		if (sscanf(spec + 1, "%1x%1x%1x", &red, &green, &blue) != 3) {
			return false;
		}
		*argb = make_argb(255, (red * 255) / 15, (green * 255) / 15,
			(blue * 255) / 15);
		return true;
	case 6:
		if (sscanf(spec + 1, "%2x%2x%2x", &red, &green, &blue) != 3) {
			return false;
		}
		*argb = make_argb(255, red, green, blue);
		return true;
	case 9:
		if (sscanf(spec + 1, "%3x%3x%3x", &red, &green, &blue) != 3) {
			return false;
		}
		*argb = make_argb(255, (red * 255) / 4095, (green * 255) / 4095,
			(blue * 255) / 4095);
		return true;
	case 12:
		if (sscanf(spec + 1, "%4x%4x%4x", &red, &green, &blue) != 3) {
			return false;
		}
		*argb = make_argb(255, (red * 255) / 65535,
			(green * 255) / 65535, (blue * 255) / 65535);
		return true;
	default:
		return false;
	}
}

static bool
xpm_seek_string(FILE *infile, const char *str)
{
	char instr[1024];

	while (!feof(infile)) {
		if (fscanf(infile, "%1023s", instr) < 0) {
			return false;
		}
		if (strcmp(instr, str) == 0) {
			return true;
		}
	}

	return false;
}

static bool
xpm_seek_char(FILE *infile, char c)
{
	int b, oldb;

	while ((b = getc(infile)) != EOF) {
		if (c != b && b == '/') {
			b = getc(infile);
			if (b == EOF) {
				return false;
			} else if (b == '*') { /* we have a comment */
				b = -1;
				do {
					oldb = b;
					b = getc(infile);
					if (b == EOF) {
						return false;
					}
				} while (!(oldb == '*' && b == '/'));
			}
		} else if (c == b) {
			return true;
		}
	}

	return false;
}

static bool
xpm_read_string(FILE *infile, struct buf *buf)
{
	buf_clear(buf);
	int c;

	do {
		c = getc(infile);
		if (c == EOF) {
			return false;
		}
	} while (c != '"');

	while ((c = getc(infile)) != EOF) {
		if (c == '"') {
			return true;
		}
		buf_add_char(buf, c);
	}

	return false;
}

static uint32_t
xpm_extract_color(const char *buffer)
{
	const char *p = buffer;
	int new_key = 0;
	int key = 0;
	int current_key = 1;
	char word[129], color[129], current_color[129];
	char *r;

	word[0] = '\0';
	color[0] = '\0';
	current_color[0] = '\0';
	while (true) {
		/* skip whitespace */
		for (; *p != '\0' && g_ascii_isspace(*p); p++) {
			/* nothing */
		}
		/* copy word */
		for (r = word; *p != '\0' && !g_ascii_isspace(*p)
				&& r - word < (int)sizeof(word) - 1;
				p++, r++) {
			*r = *p;
		}
		*r = '\0';
		if (*word == '\0') {
			if (color[0] == '\0') { /* incomplete colormap entry */
				return 0;
			} else { /* end of entry, still store the last color */
				new_key = 1;
			}
		} else if (key > 0 && color[0] == '\0') {
			/* next word must be a color name part */
			new_key = 0;
		} else {
			if (strcmp(word, "c") == 0) {
				new_key = 5;
			} else if (strcmp(word, "g") == 0) {
				new_key = 4;
			} else if (strcmp(word, "g4") == 0) {
				new_key = 3;
			} else if (strcmp(word, "m") == 0) {
				new_key = 2;
			} else if (strcmp(word, "s") == 0) {
				new_key = 1;
			} else {
				new_key = 0;
			}
		}
		if (new_key == 0) {	/* word is a color name part */
			if (key == 0) { /* key expected */
				return 0;
			}
			/* accumulate color name */
			int len = strlen(color);
			if (len && len < (int)sizeof(color) - 1) {
				color[len++] = ' ';
			}
			g_strlcpy(color + len, word, sizeof(color) - len);
		} else { /* word is a key */
			if (key > current_key) {
				current_key = key;
				g_strlcpy(current_color, color, sizeof(current_color));
			}
			color[0] = '\0';
			key = new_key;
			if (*p == '\0') {
				break;
			}
		}
	}

	uint32_t argb;
	if (current_key > 1 && (g_ascii_strcasecmp(current_color, "None") != 0)
			&& parse_color(current_color, &argb)) {
		return argb;
	} else {
		return 0;
	}
}

static const char *
file_buffer(enum buf_op op, struct file_handle *h)
{
	switch (op) {
	case op_header:
		if (!xpm_seek_string(h->infile, "XPM")) {
			break;
		}
		if (!xpm_seek_char(h->infile, '{')) {
			break;
		}
		/* Fall through to the next xpm_seek_char. */

	case op_cmap:
		xpm_seek_char(h->infile, '"');
		if (fseek(h->infile, -1, SEEK_CUR) != 0) {
			return NULL;
		}
		/* Fall through to the xpm_read_string. */

	case op_body:
		if (!xpm_read_string(h->infile, &h->buf)) {
			return NULL;
		}
		return h->buf.data;

	default:
		g_assert_not_reached();
	}

	return NULL;
}

/* This function does all the work. */
static struct lab_data_buffer *
pixbuf_create_from_xpm(struct file_handle *handle)
{
	const char *buffer = file_buffer(op_header, handle);
	if (!buffer) {
		wlr_log(WLR_DEBUG, "No XPM header found");
		return NULL;
	}

	int w, h, n_col, cpp, x_hot, y_hot;
	int items = sscanf(buffer, "%d %d %d %d %d %d", &w, &h, &n_col, &cpp,
		&x_hot, &y_hot);

	if (items != 4 && items != 6) {
		wlr_log(WLR_DEBUG, "Invalid XPM header");
		return NULL;
	}

	if (w <= 0) {
		wlr_log(WLR_DEBUG, "XPM file has image width <= 0");
		return NULL;
	}
	if (h <= 0) {
		wlr_log(WLR_DEBUG, "XPM file has image height <= 0");
		return NULL;
	}
	/* Limits (width, height, colors) modified for labwc */
	if (h > 1024 || w > 1024) {
		wlr_log(WLR_DEBUG, "XPM file is larger than 1024x1024");
		return NULL;
	}
	if (cpp <= 0 || cpp >= 32) {
		wlr_log(WLR_DEBUG, "XPM has invalid number of chars per pixel");
		return NULL;
	}
	if (n_col <= 0 || n_col > 1024) {
		wlr_log(WLR_DEBUG, "XPM file has invalid number of colors");
		return NULL;
	}

	/* The hash is used for fast lookups of color from chars */
	GHashTable *color_hash = g_hash_table_new(g_str_hash, g_str_equal);

	char *name_buf = xzalloc(n_col * (cpp + 1));
	struct xpm_color *colors = znew_n(struct xpm_color, n_col);
	uint32_t *data = znew_n(uint32_t, w * h);
	struct xpm_color *fallbackcolor = NULL;
	char pixel_str[32]; /* cpp < 32 */

	for (int cnt = 0; cnt < n_col; cnt++) {
		buffer = file_buffer(op_cmap, handle);
		if (!buffer) {
			wlr_log(WLR_DEBUG, "Cannot read XPM colormap");
			goto out;
		}

		struct xpm_color *color = &colors[cnt];
		color->color_string = &name_buf[cnt * (cpp + 1)];
		g_strlcpy(color->color_string, buffer, cpp + 1);
		buffer += strlen(color->color_string);

		color->argb = xpm_extract_color(buffer);

		g_hash_table_insert(color_hash, color->color_string, color);

		if (cnt == 0) {
			fallbackcolor = color;
		}
	}

	for (int ycnt = 0; ycnt < h; ycnt++) {
		uint32_t *pixtmp = data + w * ycnt;
		int wbytes = w * cpp;

		buffer = file_buffer(op_body, handle);
		if (!buffer || (strlen(buffer) < (size_t)wbytes)) {
			/* Advertised width doesn't match pixels */
			wlr_log(WLR_DEBUG, "Dimensions do not match data");
			goto out;
		}

		for (int n = 0, xcnt = 0; n < wbytes; n += cpp, xcnt++) {
			g_strlcpy(pixel_str, &buffer[n], cpp + 1);

			struct xpm_color *color =
				g_hash_table_lookup(color_hash, pixel_str);

			/* Bad XPM...punt */
			if (!color) {
				color = fallbackcolor;
			}

			*pixtmp++ = color->argb;
		}
	}

	g_hash_table_destroy(color_hash);
	free(colors);
	free(name_buf);

	return buffer_create_from_data(data, w, h, 4 * w);

out:
	g_hash_table_destroy(color_hash);
	free(colors);
	free(name_buf);
	free(data);

	return NULL;
}

void
img_xpm_load(const char *filename, struct lab_data_buffer **buffer)
{
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}

	struct file_handle h = {0};
	h.infile = fopen(filename, "rb");
	if (!h.infile) {
		wlr_log(WLR_ERROR, "error opening '%s'", filename);
		return;
	}

	*buffer = pixbuf_create_from_xpm(&h);
	if (!(*buffer)) {
		wlr_log(WLR_ERROR, "error loading '%s'", filename);
	}

	fclose(h.infile);
	buf_reset(&h.buf);
}
