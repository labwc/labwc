#include "config.h"
#include <cairo.h>
#include <drm_fourcc.h>
#include <pango/pangocairo.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/font.h"
#include "config/rcxml.h"
#include "labwc.h"

#define OSD_ITEM_HEIGHT (20)
#define OSD_ITEM_WIDTH (600)
#define OSD_ITEM_PADDING (10)
#define OSD_BORDER_WIDTH (6)
#define OSD_TAB1 (120)
#define OSD_TAB2 (300)

static void
set_source(cairo_t *cairo, float *c)
{
	cairo_set_source_rgba(cairo, c[0], c[1], c[2], c[3]);
}

/* is title different from app_id/class? */
static int
is_title_different(struct view *view)
{
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		return strcmp(view->impl->get_string_prop(view, "title"),
			view->impl->get_string_prop(view, "app_id"));
#if HAVE_XWAYLAND
	case LAB_XWAYLAND_VIEW:
		return strcmp(view->impl->get_string_prop(view, "title"),
			view->xwayland_surface->class);
#endif
	}
	return 1;
}

static const char *
get_formatted_app_id(struct view *view)
{
	char *s = (char *)view->impl->get_string_prop(view, "app_id");
	/* remove the first two nodes of 'org.' strings */
	if (!strncmp(s, "org.", 4)) {
		char *p = s + 4;
		p = strchr(p, '.');
		if (p) {
			return ++p;
		}
	}
	return s;
}

static int
get_osd_height(struct wl_list *views)
{
	int height = 0;
	struct view *view;
	wl_list_for_each(view, views, link) {
		if (!isfocusable(view)) {
			continue;
		}
		height += OSD_ITEM_HEIGHT;
	}
	height += 2 * OSD_BORDER_WIDTH;
	return height;
}

void
osd_update(struct server *server)
{
	struct wlr_renderer *renderer = server->renderer;
	int w = OSD_ITEM_WIDTH + 2 * OSD_BORDER_WIDTH;
	int h = get_osd_height(&server->views);

	cairo_surface_t *surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t *cairo = cairo_create(surf);

	/* background */
	set_source(cairo, (float[4]){1.0f, 1.0f, 1.0f, 1.0f});
	cairo_rectangle(cairo, 0, 0, w, h);
	cairo_fill(cairo);

	/* highlight current application */
	int y = OSD_BORDER_WIDTH;
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!isfocusable(view)) {
			continue;
		}
		if (view == server->cycle_view) {
			set_source(cairo, (float[4]){0.3f, 0.3f, 0.3f, 0.5f});
			cairo_rectangle(cairo, OSD_BORDER_WIDTH, y,
				OSD_ITEM_WIDTH, OSD_ITEM_HEIGHT);
			cairo_fill(cairo);
			break;
		}
		y += OSD_ITEM_HEIGHT;
	}

	/* text */
	set_source(cairo, (float[4]){0.0f, 0.0f, 0.0f, 1.0f});
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	pango_layout_set_width(layout, w * PANGO_SCALE);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

	/* TODO: use font description from config */
	PangoFontDescription *desc =
		pango_font_description_from_string("sans 10");
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	PangoTabArray *tabs = pango_tab_array_new_with_positions(2, TRUE,
		PANGO_TAB_LEFT, OSD_TAB1, PANGO_TAB_LEFT, OSD_TAB2);
	pango_layout_set_tabs(layout, tabs);
	pango_tab_array_free(tabs);

	pango_cairo_update_layout(cairo, layout);

	struct buf buf;
	buf_init(&buf);
	y = OSD_BORDER_WIDTH;

	/* vertically center align */
	y += (OSD_ITEM_HEIGHT - font_height("sans 10")) / 2;

	wl_list_for_each(view, &server->views, link) {
		if (!isfocusable(view)) {
			continue;
		}
		buf.len = 0;
		cairo_move_to(cairo, OSD_BORDER_WIDTH + OSD_ITEM_PADDING, y);

		switch (view->type) {
		case LAB_XDG_SHELL_VIEW:
			buf_add(&buf, "[xdg-shell]\t");
			buf_add(&buf, get_formatted_app_id(view));
			buf_add(&buf, "\t");
			break;
#if HAVE_XWAYLAND
		case LAB_XWAYLAND_VIEW:
			buf_add(&buf, "[xwayland]\t");
			buf_add(&buf, view->impl->get_string_prop(view, "class"));
			buf_add(&buf, "\t");
			break;
#endif
		}

		if (is_title_different(view)) {
			buf_add(&buf, view->impl->get_string_prop(view, "title"));
		}

		pango_layout_set_text(layout, buf.buf, -1);
		pango_cairo_show_layout(cairo, layout);
		y += OSD_ITEM_HEIGHT;
	}

	g_object_unref(layout);

	/* convert to wlr_texture */
	cairo_surface_flush(surf);
	unsigned char *data = cairo_image_surface_get_data(surf);
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer,
		DRM_FORMAT_ARGB8888, cairo_image_surface_get_stride(surf),
		w, h, data);

	cairo_destroy(cairo);
	cairo_surface_destroy(surf);
	if (server->osd) {
		wlr_texture_destroy(server->osd);
	}
	server->osd = texture;
}
