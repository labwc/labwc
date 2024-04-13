// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include "buffer.h"
#include "common/graphic-helpers.h"
#include "common/mem.h"

static void
multi_rect_destroy_notify(struct wl_listener *listener, void *data)
{
	struct multi_rect *rect = wl_container_of(listener, rect, destroy);
	free(rect);
}

struct multi_rect *
multi_rect_create(struct wlr_scene_tree *parent, float *colors[3], int line_width)
{
	struct multi_rect *rect = znew(*rect);
	rect->line_width = line_width;
	rect->tree = wlr_scene_tree_create(parent);
	rect->destroy.notify = multi_rect_destroy_notify;
	wl_signal_add(&rect->tree->node.events.destroy, &rect->destroy);
	for (size_t i = 0; i < 3; i++) {
		rect->top[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		rect->right[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		rect->bottom[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		rect->left[i] = wlr_scene_rect_create(rect->tree, 0, 0, colors[i]);
		wlr_scene_node_set_position(&rect->top[i]->node,
			i * line_width, i * line_width);
		wlr_scene_node_set_position(&rect->left[i]->node,
			i * line_width, (i + 1) * line_width);
	}
	return rect;
}

void
multi_rect_set_size(struct multi_rect *rect, int width, int height)
{
	assert(rect);
	int line_width = rect->line_width;

	/*
	 * The outmost outline is drawn like below:
	 *
	 * |--width--|
	 *
	 * +---------+  ---
	 * +-+-----+-+   |
	 * | |     | | height
	 * | |     | |   |
	 * +-+-----+-+   |
	 * +---------+  ---
	 */
	for (size_t i = 0; i < 3; i++) {
		/* Reposition, top and left don't ever change */
		wlr_scene_node_set_position(&rect->right[i]->node,
			width - (i + 1) * line_width, (i + 1) * line_width);
		wlr_scene_node_set_position(&rect->bottom[i]->node,
			i * line_width, height - (i + 1) * line_width);

		/* Update sizes */
		wlr_scene_rect_set_size(rect->top[i],
			width - i * line_width * 2, line_width);
		wlr_scene_rect_set_size(rect->bottom[i],
			width - i * line_width * 2, line_width);
		wlr_scene_rect_set_size(rect->left[i],
			line_width, height - (i + 1) * line_width * 2);
		wlr_scene_rect_set_size(rect->right[i],
			line_width, height - (i + 1) * line_width * 2);
	}
}

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

struct surface_context
get_cairo_surface_from_lab_data_buffer(struct lab_data_buffer *buffer)
{
	/* Handle CAIRO_FORMAT_ARGB32 buffers */
	if (buffer->cairo) {
		return (struct surface_context){
			.is_duplicate = false,
			.surface = cairo_get_target(buffer->cairo),
		};
	}

	/* Handle DRM_FORMAT_ARGB8888 buffers */
	int w = buffer->unscaled_width;
	int h = buffer->unscaled_height;
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (!surface) {
		return (struct surface_context){0};
	}
	unsigned char *data = cairo_image_surface_get_data(surface);
	cairo_surface_flush(surface);
	memcpy(data, buffer->data, h * buffer->stride);
	cairo_surface_mark_dirty(surface);
	return (struct surface_context){
		.is_duplicate = true,
		.surface = surface,
	};
}
