/*
 * Helpers for view server side decorations
 *
 * Copyright (C) 2020 Johan Malm
 */

#include <assert.h>
#include <cairo/cairo.h>
#include <drm_fourcc.h>
#include <math.h>
#include "config/rcxml.h"
#include "labwc.h"
#include "theme.h"
#include "ssd.h"

struct border
ssd_thickness(struct view *view)
{
	struct theme *theme = view->server->theme;
	struct border border = {
		.top = rc.title_height + theme->border_width,
		.bottom = theme->border_width,
		.left = theme->border_width,
		.right = theme->border_width,
	};
	return border;
}

struct wlr_box
ssd_max_extents(struct view *view)
{
	struct border border = ssd_thickness(view);
	struct wlr_box box = {
		.x = view->x - border.left,
		.y = view->y - border.top,
		.width = view->w + border.left + border.right,
		.height = view->h + border.top + border.bottom,
	};
	return box;
}

struct wlr_box
ssd_box(struct view *view, enum ssd_part_type type)
{
	struct theme *theme = view->server->theme;
	struct wlr_box box = { 0 };
	int corner_square = rc.title_height + theme->border_width;
	assert(view);
	switch (type) {
	case LAB_SSD_BUTTON_CLOSE:
		box.x = view->x + view->w - rc.title_height;
		box.y = view->y - rc.title_height;
		box.width = rc.title_height;
		box.height = rc.title_height;
		break;
	case LAB_SSD_BUTTON_MAXIMIZE:
		box.x = view->x + view->w - rc.title_height * 2;
		box.y = view->y - rc.title_height;
		box.width = rc.title_height;
		box.height = rc.title_height;
		break;
	case LAB_SSD_BUTTON_ICONIFY:
		box.x = view->x + view->w - rc.title_height * 3;
		box.y = view->y - rc.title_height;
		box.width = rc.title_height;
		box.height = rc.title_height;
		break;
	case LAB_SSD_PART_TITLE:
		box.x = view->x + rc.title_height;
		box.y = view->y - rc.title_height;
		box.width = view->w - 2 * rc.title_height;
		box.height = rc.title_height;
		break;
	case LAB_SSD_PART_TOP:
		box.x = view->x + rc.title_height;
		box.y = view->y - corner_square;
		box.width = view->w - 2 * rc.title_height;
		box.height = theme->border_width;
		break;
	case LAB_SSD_PART_RIGHT:
		box.x = view->x + view->w;
		box.y = view->y;
		box.width = theme->border_width;
		box.height = view->h;
		break;
	case LAB_SSD_PART_BOTTOM:
		box.x = view->x - theme->border_width;
		box.y = view->y + view->h;
		box.width = view->w + 2 * theme->border_width;
		box.height = +theme->border_width;
		break;
	case LAB_SSD_PART_LEFT:
		box.x = view->x - theme->border_width;
		box.y = view->y;
		box.width = theme->border_width;
		box.height = view->h;
		break;
	case LAB_SSD_PART_CORNER_TOP_LEFT:
		box.x = view->x - theme->border_width;
		box.y = view->y - corner_square;
		box.width = corner_square;
		box.height = corner_square;
		break;
	case LAB_SSD_PART_CORNER_TOP_RIGHT:
		box.x = view->x + view->w - rc.title_height;
		box.y = view->y - corner_square;
		box.width = corner_square;
		box.height = corner_square;
		break;
	default:
		break;
	}
	return box;
}

enum ssd_part_type
ssd_at(struct view *view, double lx, double ly)
{
	enum ssd_part_type type;
	for (type = 0; type < LAB_SSD_END_MARKER; ++type) {
		struct wlr_box box = ssd_box(view, type);
		if (wlr_box_contains_point(&box, lx, ly)) {
			return type;
		}
	}
	return LAB_SSD_NONE;
}

static struct ssd_part *
add_part(struct view *view, enum ssd_part_type type)
{
	struct ssd_part *part = calloc(1, sizeof(struct ssd_part));
	part->type = type;
	wl_list_insert(&view->ssd.parts, &part->link);
	return part;
}

struct rounded_corner_ctx {
	struct wlr_box *box;
	double radius;
	double line_width;
	float *fill_color;
	float *border_color;
	enum {
		LAB_CORNER_UNKNOWN = 0,
		LAB_CORNER_TOP_LEFT,
		LAB_CORNER_TOP_RIGHT,
	} corner;
};

static void set_source(cairo_t *cairo, float *c)
{
	cairo_set_source_rgba(cairo, c[0], c[1], c[2], c[3]);
}

static struct wlr_texture *
rounded_rect(struct wlr_renderer *renderer, struct rounded_corner_ctx *ctx)
{
	/* 1 degree in radians (=2π/360) */
	double deg = 0.017453292519943295;

	if (ctx->corner == LAB_CORNER_UNKNOWN) {
		return NULL;
	}

	double w = ctx->box->width;
	double h = ctx->box->height;
	double r = ctx->radius;

	cairo_surface_t *surf =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t *cairo = cairo_create(surf);

	/* set transparent background */
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);

	/* fill */
	cairo_set_line_width(cairo, 0.0);
	cairo_new_sub_path(cairo);
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT:
		cairo_arc(cairo, r, r, r, 180 * deg, 270 * deg);
		cairo_line_to(cairo, w, 0);
		cairo_line_to(cairo, w, h);
		cairo_line_to(cairo, 0, h);
		break;
	case LAB_CORNER_TOP_RIGHT:
		cairo_arc(cairo, w - r, r, r, -90 * deg, 0 * deg);
		cairo_line_to(cairo, w, h);
		cairo_line_to(cairo, 0, h);
		cairo_line_to(cairo, 0, 0);
		break;
	default:
		warn("unknown corner type");
	}
	cairo_close_path(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	set_source(cairo, ctx->fill_color);
	cairo_fill_preserve(cairo);
	cairo_stroke(cairo);

	/* border */
	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_ROUND);
	set_source(cairo, ctx->border_color);
	cairo_set_line_width(cairo, ctx->line_width);
	double half_line_width = ctx->line_width / 2.0;
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT:
		cairo_move_to(cairo, half_line_width, h);
		cairo_line_to(cairo, half_line_width, r + half_line_width);
		cairo_arc(cairo, r, r, r - half_line_width, 180 * deg, 270 * deg);
		cairo_line_to(cairo, w, half_line_width);
		break;
	case LAB_CORNER_TOP_RIGHT:
		cairo_move_to(cairo, 0, half_line_width);
		cairo_line_to(cairo, w - r, half_line_width);
		cairo_arc(cairo, w - r, r, r - half_line_width, -90 * deg, 0 * deg);
		cairo_line_to(cairo, w - half_line_width, h);
		break;
	default:
		warn("unknown corner type");
	}
	cairo_stroke(cairo);

	/* convert to wlr_texture */
	cairo_surface_flush(surf);
	unsigned char *data = cairo_image_surface_get_data(surf);
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer,
		DRM_FORMAT_ARGB8888, cairo_image_surface_get_stride(surf),
		w, h, data);

	cairo_destroy(cairo);
	cairo_surface_destroy(surf);
	return texture;
}

void
ssd_create(struct view *view)
{
	struct theme *theme = view->server->theme;
	struct ssd_part *part;

	view->ssd.box.x = view->x;
	view->ssd.box.y = view->y;
	view->ssd.box.width = view->w;
	view->ssd.box.height = view->h;

	/* border */
	enum ssd_part_type border[4] = {
		LAB_SSD_PART_TOP,
		LAB_SSD_PART_RIGHT,
		LAB_SSD_PART_BOTTOM,
		LAB_SSD_PART_LEFT,
	};
	for (int i = 0; i < 4; i++) {
		part = add_part(view, border[i]);
		part->box = ssd_box(view, border[i]);
		part->color.active = theme->window_active_border_color;
		part->color.inactive = theme->window_active_border_color;
	}

	/* titlebar */
	part = add_part(view, LAB_SSD_PART_TITLE);
	part->box = ssd_box(view, LAB_SSD_PART_TITLE);
	part->color.active = theme->window_active_title_bg_color;
	part->color.inactive = theme->window_inactive_title_bg_color;

	/* titlebar top left corner */
	struct wlr_renderer *renderer = view->server->renderer;
	part = add_part(view, LAB_SSD_PART_CORNER_TOP_LEFT);
	part->box = ssd_box(view, part->type);
	struct rounded_corner_ctx ctx = {
		.box = &part->box,
		.radius = rc.corner_radius,
		.line_width = theme->border_width,
		.fill_color = theme->window_active_title_bg_color,
		.border_color = theme->window_active_border_color,
		.corner = LAB_CORNER_TOP_LEFT,
	};
	part->texture.active = rounded_rect(renderer, &ctx);

	ctx.fill_color = theme->window_inactive_title_bg_color,
	part->texture.inactive = rounded_rect(renderer, &ctx);

	/* titlebar top right corner */
	part = add_part(view, LAB_SSD_PART_CORNER_TOP_RIGHT);
	part->box = ssd_box(view, part->type);
	ctx.box = &part->box;
	ctx.corner = LAB_CORNER_TOP_RIGHT;
	ctx.fill_color = theme->window_active_title_bg_color,
	part->texture.active = rounded_rect(renderer, &ctx);

	ctx.fill_color = theme->window_inactive_title_bg_color,
	part->texture.inactive = rounded_rect(renderer, &ctx);
}

void
ssd_destroy(struct view *view)
{
	struct ssd_part *part, *next;
	wl_list_for_each_safe(part, next, &view->ssd.parts, link) {
		wl_list_remove(&part->link);
		free(part);
	}
}

static bool
geometry_changed(struct view *view)
{
	return view->x != view->ssd.box.x || view->y != view->ssd.box.y ||
		view->w != view->ssd.box.width ||
		view->h != view->ssd.box.height;
}

void
ssd_update_geometry(struct view *view)
{
	if (!geometry_changed(view)) {
		return;
	}
	struct ssd_part *part;
	wl_list_for_each(part, &view->ssd.parts, link) {
		part->box = ssd_box(view, part->type);
	}
	view->ssd.box.x = view->x;
	view->ssd.box.y = view->y;
	view->ssd.box.width = view->w;
	view->ssd.box.height = view->h;
	damage_all_outputs(view->server);
}
