// SPDX-License-Identifier: GPL-2.0-only

#include <cairo.h>
#include <glib.h> /* g_ascii_strcasecmp */
#include <wlr/types/wlr_scene.h>
#include "common/graphic-helpers.h"
#include "common/macros.h"
#include "xcolor-table.h"

/* Draws a border with a specified line width */
void
draw_cairo_border(cairo_t *cairo, struct wlr_fbox fbox, double line_width)
{
	cairo_save(cairo);

	/* The anchor point of a line is in the center */
	fbox.x += line_width / 2.0;
	fbox.y += line_width / 2.0;
	fbox.width -= line_width;
	fbox.height -= line_width;
	cairo_set_line_width(cairo, line_width);
	cairo_rectangle(cairo, fbox.x, fbox.y, fbox.width, fbox.height);
	cairo_stroke(cairo);

	cairo_restore(cairo);
}

/* Sets the cairo color. Splits the single color channels */
void
set_cairo_color(cairo_t *cairo, const float *c)
{
	/*
	 * We are dealing with pre-multiplied colors
	 * but cairo expects unmultiplied colors here
	 */
	float alpha = c[3];

	if (alpha == 0.0f) {
		cairo_set_source_rgba(cairo, 0, 0, 0, 0);
		return;
	}

	cairo_set_source_rgba(cairo, c[0] / alpha, c[1] / alpha,
		c[2] / alpha, alpha);
}

static int
compare_xcolor_entry(const void *a, const void *b)
{
	/* using ASCII version to avoid locale-dependent ordering */
	return g_ascii_strcasecmp((const char *)a,
		color_names + ((const struct xcolor_entry *)b)->name_offset);
}

bool
lookup_named_color(const char *name, uint32_t *argb)
{
	struct xcolor_entry *found = bsearch(name, xcolors, ARRAY_SIZE(xcolors),
		sizeof(struct xcolor_entry), compare_xcolor_entry);
	if (!found) {
		return false;
	}

	*argb = 0xFF000000u | ((uint32_t)found->red << 16)
		| ((uint32_t)found->green << 8) | found->blue;
	return true;
}
