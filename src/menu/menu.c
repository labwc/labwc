#define _POSIX_C_SOURCE 200809L
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/font.h"
#include "labwc.h"
#include "menu/menu.h"
#include "theme/theme.h"

static const char font[] = "Sans 8";

#define MENUWIDTH (100)
#define MENUHEIGHT (25)
#define MENU_PADDING_WIDTH (7)

struct wlr_texture *
texture_create(struct server *server, struct wlr_box *geo, const char *text,
		float *bg, float *fg)
{
	cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		geo->width, geo->height);
	cairo_t *cairo = cairo_create(surf);

	cairo_set_source_rgb(cairo, bg[0], bg[1], bg[2]);
	cairo_paint(cairo);
	cairo_set_source_rgba(cairo, fg[0], fg[1], fg[2], fg[3]);
	cairo_move_to(cairo, 0, 0);

	PangoLayout *layout = pango_cairo_create_layout(cairo);
	pango_layout_set_width(layout, geo->width * PANGO_SCALE);
	pango_layout_set_text(layout, text, -1);

	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_cairo_update_layout(cairo, layout);

	/* center-align vertically */
	int height;
	pango_layout_get_pixel_size(layout, NULL, &height);
	cairo_move_to(cairo, MENU_PADDING_WIDTH, (geo->height - height) / 2);

	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);

	cairo_surface_flush(surf);
	unsigned char *data = cairo_image_surface_get_data(surf);
	struct wlr_texture *texture = wlr_texture_from_pixels(server->renderer,
		WL_SHM_FORMAT_ARGB8888, cairo_image_surface_get_stride(surf),
		geo->width, geo->height, data);

	cairo_destroy(cairo);
	cairo_surface_destroy(surf);
	return texture;
}

struct menuitem *
menuitem_create(struct server *server, struct menu *menu, const char *text,
	const char *action, const char *command)
{
	struct menuitem *menuitem = calloc(1, sizeof(struct menuitem));
	if (!menuitem) {
		return NULL;
	}
	menuitem->action = action ? strdup(action) : NULL;
	menuitem->command = command ? strdup(command) : NULL;
	menuitem->geo_box.width = MENUWIDTH;
	menuitem->geo_box.height = MENUHEIGHT;
	menuitem->active_texture = texture_create(server, &menuitem->geo_box,
		text, theme.menu_items_active_bg_color,
		theme.menu_items_active_text_color);
	menuitem->inactive_texture = texture_create(server, &menuitem->geo_box,
		text, theme.menu_items_bg_color, theme.menu_items_text_color);
	wl_list_insert(&menu->menuitems, &menuitem->link);
	return menuitem;
}

void
menu_init(struct server *server, struct menu *menu)
{
	wl_list_init(&menu->menuitems);
	menuitem_create(server, menu, "Terminal", "Execute", "sakura");
	menuitem_create(server, menu, "Reconfigure", "Reconfigure", NULL);
	menuitem_create(server, menu, "Exit", "Exit", NULL);
	menu_move(menu, 100, 100);
}

void
menu_finish(struct menu *menu)
{
	struct menuitem *menuitem, *next;
	wl_list_for_each_safe(menuitem, next, &menu->menuitems, link) {
		if (menuitem->action)
			free(menuitem->action);
		if (menuitem->command)
			free(menuitem->command);
		if (menuitem->active_texture)
			free(menuitem->active_texture);
		if (menuitem->inactive_texture)
			free(menuitem->inactive_texture);
		wl_list_remove(&menuitem->link);
		free(menuitem);
	}
}

void
menu_move(struct menu *menu, int x, int y)
{
	menu->x = x;
	menu->y = y;

	int offset = 0;
	struct menuitem *menuitem;
	wl_list_for_each_reverse (menuitem, &menu->menuitems, link) {
		menuitem->geo_box.x = menu->x;
		menuitem->geo_box.y = menu->y + offset;
		offset += menuitem->geo_box.height;
	}
}

void
menu_set_selected(struct menu *menu, int x, int y)
{
	struct menuitem *menuitem;
	wl_list_for_each (menuitem, &menu->menuitems, link) {
		menuitem->selected =
			wlr_box_contains_point(&menuitem->geo_box, x, y);
	}
}

void
menu_action_selected(struct server *server, struct menu *menu)
{
	struct menuitem *menuitem;
	wl_list_for_each (menuitem, &menu->menuitems, link) {
		if (menuitem->selected) {
			action(server, menuitem->action, menuitem->command);
			break;
		}
	}
}
