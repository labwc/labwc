// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_output.h>
#include "magnifier.h"
#include "labwc.h"
#include "theme.h"
#include "common/macros.h"

bool magnify_on;
double mag_scale = 0.0;

#define CLAMP(in, lower, upper) MAX(MIN(in, upper), lower)

void
magnify(struct output *output, struct wlr_buffer *output_buffer, struct wlr_box *damage)
{
	int width, height;
	double x, y;
	struct wlr_box border_box, dst_box;
	struct wlr_fbox src_box;
	bool fullscreen = false;

	/* Reuse a single scratch buffer */
	static struct wlr_buffer *tmp_buffer = NULL;
	static struct wlr_texture *tmp_texture = NULL;

	/* TODO: This looks way too complicated to just get the used format */
	struct wlr_drm_format wlr_drm_format = {0};
	struct wlr_shm_attributes shm_attribs = {0};
	struct wlr_dmabuf_attributes dma_attribs = {0};
	if (wlr_buffer_get_dmabuf(output_buffer, &dma_attribs)) {
		wlr_drm_format.format = dma_attribs.format;
		wlr_drm_format.len = 1;
		wlr_drm_format.modifiers = &dma_attribs.modifier;
	} else if (wlr_buffer_get_shm(output_buffer, &shm_attribs)) {
		wlr_drm_format.format = shm_attribs.format;
	} else {
		wlr_log(WLR_ERROR, "Failed to read buffer format");
		return;
	}

	/* Fetch scale-adjusted cursor coordinates */
	struct server *server = output->server;
	struct theme *theme = server->theme;
	struct wlr_cursor *cursor = server->seat.cursor;
	double ox = cursor->x;
	double oy = cursor->y;
	wlr_output_layout_output_coords(server->output_layout, output->wlr_output, &ox, &oy);
	ox *= output->wlr_output->scale;
	oy *= output->wlr_output->scale;
	if (theme->mag_width == -1 || theme->mag_height == -1) {
		fullscreen = true;
	}
	if ((ox < 0 || oy < 0 || ox >= output_buffer->width || oy >= output_buffer->height)
		&& fullscreen) {
		return;
	}

	if (mag_scale == 0.0) {
		mag_scale = theme->mag_scale;
	}

	if (fullscreen) {
		width = output_buffer->width;
		height = output_buffer->height;
		x = 0;
		y = 0;
	} else {
		width = theme->mag_width + 1;
		height = theme->mag_height + 1;
		x = ox - (theme->mag_width / 2.0);
		y = oy - (theme->mag_height / 2.0);
	}
	double cropped_width = width;
	double cropped_height = height;
	double dst_x = 0;
	double dst_y = 0;

	/* Ensure everything is kept within output boundaries */
	if (x < 0) {
		cropped_width += x;
		dst_x = x * -1;
		x = 0;
	}
	if (y < 0) {
		cropped_height += y;
		dst_y = y * -1;
		y = 0;
	}
	cropped_width = MIN(cropped_width, (double)output_buffer->width - x);
	cropped_height = MIN(cropped_height, (double)output_buffer->height - y);

	/* (Re)create the temporary buffer if required */
	if (tmp_buffer && (tmp_buffer->width != width || tmp_buffer->height != height)) {
		wlr_log(WLR_DEBUG, "tmp buffer size changed, dropping");
		assert(tmp_texture);
		wlr_texture_destroy(tmp_texture);
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
		tmp_texture = NULL;
	}
	if (!tmp_buffer) {
		tmp_buffer = wlr_allocator_create_buffer(
			server->allocator, width, height, &wlr_drm_format);
	}
	if (!tmp_buffer) {
		wlr_log(WLR_ERROR, "Failed to allocate temporary magnifier buffer");
		return;
	}

	/* Paste the magnified result back into the output buffer */
	if (!tmp_texture) {
		tmp_texture = wlr_texture_from_buffer(server->renderer, tmp_buffer);
	}
	if (!tmp_texture) {
		wlr_log(WLR_ERROR, "Failed to allocate temporary texture");
		return;
	}

	/* Extract source region into temporary buffer */

	struct wlr_render_pass *tmp_render_pass = wlr_renderer_begin_buffer_pass(
		server->renderer, tmp_buffer, NULL);

	wlr_buffer_lock(output_buffer);
	struct wlr_texture *output_texture = wlr_texture_from_buffer(
		server->renderer, output_buffer);
	if (!output_texture) {
		goto cleanup;
	}

	struct wlr_render_texture_options opts = {
		.texture = output_texture,
		.src_box = (struct wlr_fbox) {
			x, y, cropped_width, cropped_height },
		.dst_box = (struct wlr_box) {
			dst_x, dst_y, cropped_width, cropped_height },
		.alpha = NULL,
	};
	wlr_render_pass_add_texture(tmp_render_pass, &opts);
	if (!wlr_render_pass_submit(tmp_render_pass)) {
		wlr_log(WLR_ERROR, "Failed to extract magnifier source region");
		wlr_texture_destroy(output_texture);
		goto cleanup;
	}
	wlr_texture_destroy(output_texture);

	/* Render to the output buffer itself */
	tmp_render_pass = wlr_renderer_begin_buffer_pass(
		server->renderer, output_buffer, NULL);

	/* Borders */
	if (fullscreen) {
		border_box.x = 0;
		border_box.y = 0;
		border_box.width = width;
		border_box.height = height;
	} else {
		border_box.x = ox - (width / 2 + theme->mag_border_width);
		border_box.y = oy - (height / 2 + theme->mag_border_width);
		border_box.width = (width + theme->mag_border_width * 2);
		border_box.height = (height + theme->mag_border_width * 2);
		struct wlr_render_rect_options bg_opts = {
			.box = border_box,
			.color = (struct wlr_render_color) {
				.r = theme->mag_border_color[0],
				.g = theme->mag_border_color[1],
				.b = theme->mag_border_color[2],
				.a = theme->mag_border_color[3]
			},
			.clip = NULL,
		};
		wlr_render_pass_add_rect(tmp_render_pass, &bg_opts);
	}

	src_box.width = width / mag_scale;
	src_box.height = height / mag_scale;
	dst_box.width = width;
	dst_box.height = height;

	if (fullscreen) {
		src_box.x = CLAMP(ox - (ox / mag_scale), 0.0,
			width * (mag_scale - 1.0) / mag_scale);
		src_box.y = CLAMP(oy - (oy / mag_scale), 0.0,
			height * (mag_scale - 1.0) / mag_scale);
		dst_box.x = 0;
		dst_box.y = 0;
	} else {
		src_box.x = width * (mag_scale - 1.0) / (2.0 * mag_scale);
		src_box.y = height * (mag_scale - 1.0) / (2.0 * mag_scale);
		dst_box.x = ox - (width / 2);
		dst_box.y = oy - (height / 2);
	}

	opts = (struct wlr_render_texture_options) {
		.texture = tmp_texture,
		.src_box = src_box,
		.dst_box = dst_box,
		.alpha = NULL,
		.clip = NULL,
		.filter_mode = theme->mag_filter ? WLR_SCALE_FILTER_BILINEAR
			: WLR_SCALE_FILTER_NEAREST,
	};
	wlr_render_pass_add_texture(tmp_render_pass, &opts);
	if (!wlr_render_pass_submit(tmp_render_pass)) {
		wlr_log(WLR_ERROR, "Failed to submit render pass");
		goto cleanup;
	}

	/* And finally mark the extra damage */
	*damage = border_box;
	damage->width += 1;
	damage->height += 1;

cleanup:
	wlr_buffer_unlock(output_buffer);
}

bool
output_wants_magnification(struct output *output)
{
	static double x = -1;
	static double y = -1;
	struct wlr_cursor *cursor = output->server->seat.cursor;
	if (!magnify_on) {
		x = -1;
		y = -1;
		return false;
	}
	if (cursor->x == x && cursor->y == y) {
		return false;
	}
	x = cursor->x;
	y = cursor->y;
	return output_nearest_to_cursor(output->server) == output;
}

/*
 * Toggles magnification on and off
 */

void
magnify_toggle(struct server *server)
{
	struct output *output = output_nearest_to_cursor(server);

	if (magnify_on) {
		magnify_on = false;
	} else {
		magnify_on = true;
	}

	if (output) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

/*
 * Increases and decreases magnification scale
 */

void
magnify_set_scale(struct server *server, enum magnify_dir dir)
{
	struct output *output = output_nearest_to_cursor(server);
	struct theme *theme = server->theme;

	if (dir == MAGNIFY_INCREASE) {
		if (magnify_on) {
			mag_scale += theme->mag_increment;
		} else {
			magnify_on = true;
			mag_scale = 1.0 + theme->mag_increment;
		}
	} else {
		if (magnify_on && mag_scale > 1.0 + theme->mag_increment) {
			mag_scale -= theme->mag_increment;
		} else {
			magnify_on = false;
		}
	}

	if (output) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

/*
 * Report whether magnification is enabled
 */

bool
is_magnify_on(void)
{
	return magnify_on;
}

