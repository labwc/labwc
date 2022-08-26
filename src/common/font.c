// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <pango/pangocairo.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "labwc.h"
#include "buffer.h"

static PangoRectangle
font_extents(struct font *font, const char *string)
{
	PangoRectangle rect = { 0 };
	if (!string) {
		return rect;
	}
	cairo_surface_t *surface;
	cairo_t *c;
	PangoLayout *layout;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	c = cairo_create(surface);
	layout = pango_cairo_create_layout(c);
	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc, font->name);
	pango_font_description_set_size(desc, font->size * PANGO_SCALE);

	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, string, -1);
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	pango_layout_set_width(layout, -1);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_MIDDLE);
	pango_layout_get_extents(layout, NULL, &rect);
	pango_extents_to_pixels(&rect, NULL);

	/* we put a 2 px edge on each side - because Openbox does it :) */
	rect.width += 4;

	cairo_destroy(c);
	cairo_surface_destroy(surface);
	pango_font_description_free(desc);
	g_object_unref(layout);
	return rect;
}

int
font_height(struct font *font)
{
	PangoRectangle rectangle = font_extents(font, "abcdefg");
	return rectangle.height;
}

int
font_width(struct font *font, const char *string)
{
	PangoRectangle rectangle = font_extents(font, string);
	return rectangle.width;
}

void
font_buffer_create(struct lab_data_buffer **buffer, int max_width,
	const char *text, struct font *font, float *color, const char *arrow,
	double scale)
{
	/* Allow a minimum of one pixel each for text and arrow */
	if (max_width < 2) {
		max_width = 2;
	}

	if (!text || !*text) {
		return;
	}

	PangoRectangle text_extents = font_extents(font, text);
	PangoRectangle arrow_extents = font_extents(font, arrow);

	if (arrow) {
		if (arrow_extents.width >= max_width - 1) {
			/* It would be weird to get here, but just in case */
			arrow_extents.width = max_width - 1;
			text_extents.width = 1;
		} else {
			text_extents.width = max_width - arrow_extents.width;
		}
	} else if (text_extents.width > max_width) {
		text_extents.width = max_width;
	}

	*buffer = buffer_create_cairo(text_extents.width + arrow_extents.width,
			text_extents.height, scale, true);
	if (!*buffer) {
		wlr_log(WLR_ERROR, "Failed to create font buffer");
		return;
	}

	cairo_t *cairo = (*buffer)->cairo;
	cairo_surface_t *surf = cairo_get_target(cairo);

	set_cairo_color(cairo, color);
	cairo_move_to(cairo, 0, 0);

	PangoLayout *layout = pango_cairo_create_layout(cairo);
	pango_layout_set_width(layout, text_extents.width * PANGO_SCALE);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc, font->name);
	pango_font_description_set_size(desc, font->size * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);

	if (arrow) {
		cairo_move_to(cairo, text_extents.width, 0);
		pango_layout_set_width(layout, arrow_extents.width * PANGO_SCALE);
		pango_layout_set_text(layout, arrow, -1);
		pango_cairo_show_layout(cairo, layout);
	}

	g_object_unref(layout);

	cairo_surface_flush(surf);
}

void
font_finish(void)
{
	pango_cairo_font_map_set_default(NULL);
}
