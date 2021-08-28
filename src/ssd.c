/*
 * Helpers for view server side decorations
 *
 * Copyright (C) Johan Malm 2020-2021
 */

#include <assert.h>
#include "config/rcxml.h"
#include "common/font.h"
#include "labwc.h"
#include "theme.h"
#include "ssd.h"

#define INVISIBLE_MARGIN (16)

struct border
ssd_thickness(struct view *view)
{
	struct theme *theme = view->server->theme;
	struct border border = {
		.top = theme->title_height + theme->border_width,
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

#define NR_BUTTONS (3)

/**
 * ssd_box - the 'full' decoration geometry which includes both visible
 * and invisible parts. It typically includes an invisible margin outside
 * the decoration.
 */
static struct wlr_box
ssd_box(struct view *view, enum ssd_part_type type)
{
	struct theme *theme = view->server->theme;
	struct wlr_box box = { 0 };
	int corner_square = theme->title_height + theme->border_width;
	switch (type) {
	case LAB_SSD_BUTTON_CLOSE:
		box.x = view->x + view->w - theme->title_height;
		box.y = view->y - theme->title_height;
		box.width = theme->title_height;
		box.height = theme->title_height;
		break;
	case LAB_SSD_BUTTON_MAXIMIZE:
		box.x = view->x + view->w - theme->title_height * 2;
		box.y = view->y - theme->title_height;
		box.width = theme->title_height;
		box.height = theme->title_height;
		break;
	case LAB_SSD_BUTTON_ICONIFY:
		box.x = view->x + view->w - theme->title_height * 3;
		box.y = view->y - theme->title_height;
		box.width = theme->title_height;
		box.height = theme->title_height;
		break;
	case LAB_SSD_PART_TITLEBAR:
		box.x = view->x;
		box.y = view->y - theme->title_height;
		box.width = view->w;
		box.height = theme->title_height;
		break;
	case LAB_SSD_PART_TITLE:
		box.x = view->x + corner_square / 2;
		box.y = view->y - theme->title_height;
		box.width = view->w - NR_BUTTONS * corner_square;
		box.height = theme->title_height;
		break;
	case LAB_SSD_PART_CORNER_TOP_LEFT:
		box.x = view->x - theme->border_width - INVISIBLE_MARGIN;
		box.y = view->y - corner_square - INVISIBLE_MARGIN;
		box.width = corner_square + INVISIBLE_MARGIN;
		box.height = corner_square + INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_CORNER_TOP_RIGHT:
		box.x = view->x + view->w - theme->title_height;
		box.y = view->y - corner_square - INVISIBLE_MARGIN;
		box.width = corner_square + INVISIBLE_MARGIN;
		box.height = corner_square + INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
		box.x = view->x + view->w - corner_square;
		box.y = view->y + view->h - corner_square;
		box.width = corner_square + INVISIBLE_MARGIN;
		box.height = corner_square + INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
		box.x = view->x - theme->border_width - INVISIBLE_MARGIN;
		box.y = view->y + view->h - corner_square;
		box.width = corner_square + INVISIBLE_MARGIN;
		box.height = corner_square + INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_TOP:
		box.x = view->x + theme->title_height;
		box.y = view->y - corner_square - INVISIBLE_MARGIN;
		box.width = view->w - 2 * theme->title_height;
		box.height = theme->border_width + INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_RIGHT:
		box.x = view->x + view->w;
		box.y = view->y;
		box.width = theme->border_width + INVISIBLE_MARGIN;
		box.height = view->h;
		break;
	case LAB_SSD_PART_BOTTOM:
		box.x = view->x - theme->border_width;
		box.y = view->y + view->h;
		box.width = view->w + 2 * theme->border_width;
		box.height = theme->border_width + INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_LEFT:
		box.x = view->x - theme->border_width - INVISIBLE_MARGIN;
		box.y = view->y;
		box.width = theme->border_width + INVISIBLE_MARGIN;
		box.height = view->h;
		break;
	default:
		break;
	}
	return box;
}

struct wlr_box
ssd_visible_box(struct view *view, enum ssd_part_type type)
{
	struct theme *theme = view->server->theme;
	struct wlr_box box = { 0 };
	switch (type) {
	case LAB_SSD_BUTTON_CLOSE:
		box = ssd_box(view, type);
		break;
	case LAB_SSD_BUTTON_MAXIMIZE:
		box = ssd_box(view, type);
		break;
	case LAB_SSD_BUTTON_ICONIFY:
		box = ssd_box(view, type);
		break;
	case LAB_SSD_PART_TITLEBAR:
		box = ssd_box(view, type);
		box.x += theme->title_height;
		box.width -= 2 * theme->title_height;
		break;
	case LAB_SSD_PART_TITLE:
		box = ssd_box(view, type);
		if (view->title) {
			/* center align title vertically */
			box.y += (box.height - view->title->height) / 2;
			box.width = view->title->width;
			box.height = view->title->height;
		}
		break;
	case LAB_SSD_PART_CORNER_TOP_LEFT:
		box = ssd_box(view, type);
		box.x += INVISIBLE_MARGIN;
		box.y += INVISIBLE_MARGIN;
		box.width -= INVISIBLE_MARGIN;
		box.height -= INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_CORNER_TOP_RIGHT:
		box = ssd_box(view, type);
		box.y += INVISIBLE_MARGIN;
		box.width -= INVISIBLE_MARGIN;
		box.height -= INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_TOP:
		box = ssd_box(view, type);
		box.y += INVISIBLE_MARGIN;
		box.height -= INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_RIGHT:
		box = ssd_box(view, type);
		box.width -= INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_BOTTOM:
		box = ssd_box(view, type);
		box.height -= INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_LEFT:
		box = ssd_box(view, type);
		box.x += INVISIBLE_MARGIN;
		box.width -= INVISIBLE_MARGIN;
		break;
	case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
	case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
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

uint32_t
ssd_resize_edges(enum ssd_part_type type)
{
	switch (type) {
	case LAB_SSD_PART_TOP:
		return WLR_EDGE_TOP;
	case LAB_SSD_PART_RIGHT:
		return WLR_EDGE_RIGHT;
	case LAB_SSD_PART_BOTTOM:
		return WLR_EDGE_BOTTOM;
	case LAB_SSD_PART_LEFT:
		return WLR_EDGE_LEFT;
	case LAB_SSD_PART_CORNER_TOP_LEFT:
		return WLR_EDGE_TOP | WLR_EDGE_LEFT;
	case LAB_SSD_PART_CORNER_TOP_RIGHT:
		return WLR_EDGE_RIGHT | WLR_EDGE_TOP;
	case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
		return WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;
	case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
		return WLR_EDGE_BOTTOM | WLR_EDGE_LEFT;
	default:
		break;
	}
	return 0;
}


static struct ssd_part *
add_part(struct view *view, enum ssd_part_type type)
{
	struct ssd_part *part = calloc(1, sizeof(struct ssd_part));
	part->type = type;
	wl_list_insert(&view->ssd.parts, &part->link);
	return part;
}

void
ssd_update_title(struct view *view)
{
	struct theme *theme = view->server->theme;

	struct font font = {
		.name = rc.font_name_activewindow,
		.size = rc.font_size_activewindow,
	};

	struct ssd_part *part;
	wl_list_for_each(part, &view->ssd.parts, link) {
		if (part->type == LAB_SSD_PART_TITLE) {
			part->box = ssd_box(view, part->type);
			break;
		}
	}
	if (part->type != LAB_SSD_PART_TITLE) {
		return;
	}

	int max_width = part->box.width > 0 ? part->box.width : 1000;

	/* TODO: use window.active.label.text.color here */
	font_texture_create(view->server, &view->title, max_width,
		view->impl->get_string_prop(view, "title"),
		&font, theme->menu_items_active_text_color);

	part->box = ssd_visible_box(view, part->type);
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
		part->box = ssd_visible_box(view, border[i]);
		part->color.active = theme->window_active_border_color;
		part->color.inactive = theme->window_inactive_border_color;
	}

	/* titlebar */
	part = add_part(view, LAB_SSD_PART_TITLEBAR);
	part->box = ssd_visible_box(view, LAB_SSD_PART_TITLEBAR);
	part->color.active = theme->window_active_title_bg_color;
	part->color.inactive = theme->window_inactive_title_bg_color;

	/* titlebar top-left corner */
	part = add_part(view, LAB_SSD_PART_CORNER_TOP_LEFT);
	part->box = ssd_visible_box(view, part->type);
	part->texture.active = &theme->corner_top_left_active_normal;
	part->texture.inactive = &theme->corner_top_left_inactive_normal;

	/* titlebar top-right corner */
	part = add_part(view, LAB_SSD_PART_CORNER_TOP_RIGHT);
	part->box = ssd_visible_box(view, part->type);
	part->texture.active = &theme->corner_top_right_active_normal;
	part->texture.inactive = &theme->corner_top_right_inactive_normal;

	/* title text */
	part = add_part(view, LAB_SSD_PART_TITLE);
	ssd_update_title(view);
	part->texture.active = &view->title;
	part->texture.inactive = &view->title;
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
ssd_update_geometry(struct view *view, bool force)
{
	if (!geometry_changed(view) && !force) {
		return;
	}
	struct ssd_part *part;
	wl_list_for_each(part, &view->ssd.parts, link) {
		part->box = ssd_visible_box(view, part->type);
	}
	view->ssd.box.x = view->x;
	view->ssd.box.y = view->y;
	view->ssd.box.width = view->w;
	view->ssd.box.height = view->h;
	damage_all_outputs(view->server);
}
