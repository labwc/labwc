// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/corner.h"
#include "common/graphic-helpers.h"
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/scaled-scene-buffer.h"
#include "common/scaled-rect-buffer.h"

static struct wl_list cached_buffers = WL_LIST_INIT(&cached_buffers);

static void
draw_edge(cairo_t *cairo, int w, int h, int r, uint32_t corners, enum wlr_direction edge)
{
	static const double deg = 0.017453292519943295;

	switch (edge) {
	case WLR_DIRECTION_UP:
		if (corners & LAB_CORNER_TOP_LEFT) {
			cairo_new_sub_path(cairo);
			cairo_arc(cairo, r, r, r, 180 * deg, 270 * deg);
		} else if (!cairo_has_current_point(cairo)) {
			cairo_move_to(cairo, 0, 0);
		}
		cairo_line_to(cairo, (corners & LAB_CORNER_TOP_RIGHT) ? w - r : w, 0);
		break;
	case WLR_DIRECTION_RIGHT:
		if (corners & LAB_CORNER_TOP_RIGHT) {
			cairo_arc(cairo, w - r, r, r, -90 * deg, 0 * deg);
		} else if (!cairo_has_current_point(cairo)) {
			cairo_move_to(cairo, w, 0);
		}
		cairo_line_to(cairo, w, (corners & LAB_CORNER_BOTTOM_RIGHT) ? h - r : h);
		break;
	case WLR_DIRECTION_DOWN:
		if (corners & LAB_CORNER_BOTTOM_RIGHT) {
			cairo_arc(cairo, w - r, h - r, r, 0 * deg, 90 * deg);
		} else if (!cairo_has_current_point(cairo)) {
			cairo_move_to(cairo, w, h);
		}
		cairo_line_to(cairo, (corners & LAB_CORNER_BOTTOM_LEFT) ? r : 0, h);
		break;
	case WLR_DIRECTION_LEFT:
		if (corners & LAB_CORNER_BOTTOM_LEFT) {
			cairo_arc(cairo, r, h - r, r, 90 * deg, 180 * deg);
		} else if (!cairo_has_current_point(cairo)) {
			cairo_move_to(cairo, 0, h);
		}
		cairo_line_to(cairo, 0, (corners & LAB_CORNER_TOP_LEFT) ? r : 0);
		break;
	}
}

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct scaled_rect_buffer *self = scaled_buffer->data;
	struct lab_data_buffer *buffer = buffer_create_cairo(self->width, self->height, scale);
	if (!buffer) {
		return NULL;
	}

	cairo_t *cairo = buffer->cairo;
	cairo_save(cairo);

	/* Clear background */
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);

	double w = self->width;
	double h = self->height;
	double r = self->corner_radius;

	/* Draw rounded rectangle with border */
	enum wlr_direction edges[4] = {WLR_DIRECTION_UP, WLR_DIRECTION_RIGHT,
		WLR_DIRECTION_DOWN, WLR_DIRECTION_LEFT};
	for (int i = 0; i < (int)ARRAY_SIZE(edges); i++) {
		draw_edge(cairo, w, h, r, self->rounded_corners, edges[i]);
	}
	cairo_close_path(cairo);
	set_cairo_color(cairo, self->fill_color);
	cairo_fill(cairo);

	double half_border_width = (double)self->border_width / 2;
	w -= self->border_width;
	h -= self->border_width;
	r = MAX(r - half_border_width, 0);
	cairo_translate(cairo, half_border_width, half_border_width);
	cairo_set_line_width(cairo, self->border_width);
	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_SQUARE);

	for (int i = 0; i < (int)ARRAY_SIZE(edges); i++) {
		if (self->stroked_edges & edges[i]) {
			set_cairo_color(cairo, self->border_color);
			draw_edge(cairo, w, h, r, self->rounded_corners, edges[i]);
			cairo_stroke(cairo);
		}
	}

	cairo_restore(cairo);

	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_rect_buffer *self = scaled_buffer->data;
	scaled_buffer->data = NULL;
	free(self);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a, struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_rect_buffer *a = scaled_buffer_a->data;
	struct scaled_rect_buffer *b = scaled_buffer_b->data;

	return a->width == b->width
		&& a->height == b->height
		&& a->border_width == b->border_width
		&& a->corner_radius == b->corner_radius
		&& a->rounded_corners == b->rounded_corners
		&& a->stroked_edges == b->stroked_edges
		&& !memcmp(a->fill_color, b->fill_color, sizeof(a->fill_color))
		&& !memcmp(a->border_color, b->border_color, sizeof(a->border_color));
}

static const struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

struct scaled_rect_buffer *scaled_rect_buffer_create(
	struct wlr_scene_tree *parent, int width, int height, int border_width,
	int corner_radius, uint32_t rounded_corners, uint32_t stroked_edges,
	float fill_color[4], float border_color[4])
{
	assert(parent);
	struct scaled_rect_buffer *self = znew(*self);
	struct scaled_scene_buffer *scaled_buffer = scaled_scene_buffer_create(
		parent, &impl, &cached_buffers, /* drop_buffer */ true);
	scaled_buffer->data = self;
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	self->width = MAX(width, 1);
	self->height = MAX(height, 1);
	self->border_width = border_width;
	self->corner_radius = corner_radius;
	self->rounded_corners = rounded_corners;
	self->stroked_edges = stroked_edges;
	memcpy(self->fill_color, fill_color, sizeof(self->fill_color));
	memcpy(self->border_color, border_color, sizeof(self->border_color));

	scaled_scene_buffer_invalidate_cache(scaled_buffer);

	return self;
}
