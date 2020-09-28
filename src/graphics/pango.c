#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>

void print(cairo_t *cairo, const char *text, const char *font)
	PangoLayout *layout = pango_cairo_create_layout(cairo);
PangoAttrList *attrs = pango_attr_list_new();
pango_layout_set_text(layout, text, -1);
pango_attr_list_insert(attrs, pango_attr_scale_new(1.0));
PangoFontDescription *desc = pango_font_description_from_string(font);
pango_layout_set_font_description(layout, desc);
pango_layout_set_single_paragraph_mode(layout, 1);
pango_layout_set_attributes(layout, attrs);
pango_attr_list_unref(attrs);
pango_font_description_free(desc);
cairo_font_options_t *fo = cairo_font_options_create();
cairo_get_font_options(cairo, fo);
pango_cairo_context_set_font_options(pango_layout_get_context(layout), fo);
cairo_font_options_destroy(fo);
pango_cairo_update_layout(cairo, layout);
pango_cairo_show_layout(cairo, layout);
g_object_unref(layout);
}

void
update_title_texture(struct wlr_texture **texture, const char *text)
{
	if (*texture) {
		wlr_texture_destroy(*texture);
		*texture = NULL;
	}
	if (!text)
		return;

	double scale = output->wlr_output->scale;
	int width = 0;
	int height = 0;

	cairo_surface_t *dummy_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cairo_t *c = cairo_create(dummy_surface);
	cairo_set_antialias(c, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	if (output->wlr_output->subpixel == WL_OUTPUT_SUBPIXEL_NONE)
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
	else
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_set_font_options(c, fo);
	get_text_size(c, config->font, &width, NULL, NULL, scale,
		      config->pango_markup, "%s", text);
	cairo_surface_destroy(dummy_surface);
	cairo_destroy(c);

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
	cairo_set_source_rgba(cairo, class->background[0], class->background[1],
			      class->background[2], class->background[3]);
	cairo_paint(cairo);
	PangoContext *pango = pango_cairo_create_context(cairo);
	cairo_set_source_rgba(cairo, class->text[0], class->text[1],
			      class->text[2], class->text[3]);
	cairo_move_to(cairo, 0, 0);

	pango_printf(cairo, config->font, scale, config->pango_markup, "%s",
		     text);

	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(output->wlr_output->backend);
	*texture = wlr_texture_from_pixels(renderer, WL_SHM_FORMAT_ARGB8888,
					   stride, width, height, data);
	cairo_surface_destroy(surface);
	g_object_unref(pango);
	cairo_destroy(cairo);
}

// void container_calculate_title_height(struct sway_container *container)
//{
//	cairo_t *cairo = cairo_create(NULL);
//	int height;
//	int baseline;
//	get_text_size(cairo, config->font, NULL, &height, &baseline, 1,
//			config->pango_markup, "%s", text);
//	cairo_destroy(cairo);
//	container->title_height = height;
///	container->title_baseline = baseline;
//}
