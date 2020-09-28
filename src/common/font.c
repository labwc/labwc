#include <cairo.h>
#include <pango/pangocairo.h>

#include "common/font.h"

static PangoRectangle
font_extents(const char *font_description, const char *string)
{
	PangoRectangle rect;
	cairo_surface_t *surface;
	cairo_t *c;
	PangoLayout *layout;
	PangoFontDescription *font;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	c = cairo_create(surface);
	layout = pango_cairo_create_layout(c);
	font = pango_font_description_from_string(font_description);

	pango_layout_set_font_description(layout, font);
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
	pango_font_description_free(font);
	g_object_unref(layout);
	return rect;
}

int
font_height(const char *font_description)
{
	PangoRectangle rectangle;
	rectangle = font_extents(font_description, "abcdefg");
	return rectangle.height;
}
