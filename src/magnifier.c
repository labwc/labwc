// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_output.h>
#include "common/box.h"
#include "common/macros.h"
#include "labwc.h"
#include "magnifier.h"
#include "theme.h"

static bool magnify_on;
static double mag_scale = 0.0;

/* Reuse a single scratch buffer */
static struct wlr_buffer *tmp_buffer = NULL;
static struct wlr_texture *tmp_texture = NULL;

void
magnifier_draw(struct output *output, struct wlr_buffer *output_buffer, struct wlr_box *damage)
{
	struct server *server = output->server;
	struct theme *theme = server->theme;
	bool fullscreen = (rc.mag_width == -1 || rc.mag_height == -1);

	struct wlr_box output_box = {
		.width = output_buffer->width,
		.height = output_buffer->height,
	};

	/* Cursor position in physical output coordinate */
	double cursor_x = server->seat.cursor->x;
	double cursor_y = server->seat.cursor->y;
	wlr_output_layout_output_coords(server->output_layout,
		output->wlr_output, &cursor_x, &cursor_y);
	cursor_x *= output->wlr_output->scale;
	cursor_y *= output->wlr_output->scale;

	bool cursor_in_output = wlr_box_contains_point(&output_box,
		cursor_x, cursor_y);
	if (fullscreen && !cursor_in_output) {
		return;
	}

	if (mag_scale == 0.0) {
		mag_scale = rc.mag_scale;
	}
	assert(mag_scale >= 1.0);

	/* Magnifier geometry in physical output coordinate */
	struct wlr_box mag_box;
	if (fullscreen) {
		mag_box = output_box;
	} else {
		mag_box.x = cursor_x - (rc.mag_width / 2.0);
		mag_box.y = cursor_y - (rc.mag_height / 2.0);
		mag_box.width = rc.mag_width;
		mag_box.height = rc.mag_height;
	}

	/* (Re)create the temporary buffer if required */
	if (tmp_buffer && (tmp_buffer->width != mag_box.width
			|| tmp_buffer->height != mag_box.height)) {
		wlr_log(WLR_DEBUG, "tmp magnifier buffer size changed, dropping");
		assert(tmp_texture);
		wlr_texture_destroy(tmp_texture);
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
		tmp_texture = NULL;
	}
	if (!tmp_buffer) {
		tmp_buffer = wlr_allocator_create_buffer(
			server->allocator, mag_box.width, mag_box.height,
			&output->wlr_output->swapchain->format);
	}
	if (!tmp_buffer) {
		wlr_log(WLR_ERROR, "Failed to allocate temporary magnifier buffer");
		return;
	}

	if (!tmp_texture) {
		tmp_texture = wlr_texture_from_buffer(server->renderer, tmp_buffer);
	}
	if (!tmp_texture) {
		wlr_log(WLR_ERROR, "Failed to allocate temporary magnifier texture");
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
		return;
	}

	/* Extract source region into temporary buffer */
	struct wlr_render_pass *tmp_render_pass = wlr_renderer_begin_buffer_pass(
		server->renderer, tmp_buffer, NULL);
	if (!tmp_render_pass) {
		wlr_log(WLR_ERROR, "Failed to begin magnifier render pass");
		return;
	}

	wlr_buffer_lock(output_buffer);
	struct wlr_texture *output_texture = wlr_texture_from_buffer(
		server->renderer, output_buffer);
	if (!output_texture) {
		goto cleanup;
	}

	struct wlr_box src_box_for_copy;
	wlr_box_intersection(&src_box_for_copy, &mag_box, &output_box);

	struct wlr_box dst_box_for_copy = src_box_for_copy;
	dst_box_for_copy.x -= mag_box.x;
	dst_box_for_copy.y -= mag_box.y;

	struct wlr_render_texture_options opts = {
		.texture = output_texture,
		.src_box = box_to_fbox(&src_box_for_copy),
		.dst_box = dst_box_for_copy,
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
	if (!tmp_render_pass) {
		wlr_log(WLR_ERROR, "Failed to begin second magnifier render pass");
		goto cleanup;
	}

	struct wlr_box damage_box;
	if (fullscreen) {
		damage_box = output_box;
	} else {
		/* Draw borders */
		struct wlr_box border_box = {
			.x = mag_box.x - theme->mag_border_width,
			.y = mag_box.y - theme->mag_border_width,
			.width = mag_box.width + theme->mag_border_width * 2,
			.height = mag_box.height + theme->mag_border_width * 2,
		};
		struct wlr_render_rect_options bg_opts = {
			.box = border_box,
			.color = (struct wlr_render_color) {
				.r = theme->mag_border_color[0],
				.g = theme->mag_border_color[1],
				.b = theme->mag_border_color[2],
				.a = theme->mag_border_color[3]
			},
		};
		wlr_render_pass_add_rect(tmp_render_pass, &bg_opts);
		wlr_box_intersection(&damage_box, &border_box, &output_box);
	}

	struct wlr_fbox src_box_for_paste = {
		.width = mag_box.width / mag_scale,
		.height = mag_box.height / mag_scale,
	};

	if (fullscreen) {
		src_box_for_paste.x = cursor_x - (cursor_x / mag_scale);
		src_box_for_paste.y = cursor_y - (cursor_y / mag_scale);
	} else {
		src_box_for_paste.x =
			mag_box.width * (mag_scale - 1.0) / (2.0 * mag_scale);
		src_box_for_paste.y =
			mag_box.height * (mag_scale - 1.0) / (2.0 * mag_scale);
	}

	/* Paste the magnified result back into the output buffer */
	opts = (struct wlr_render_texture_options) {
		.texture = tmp_texture,
		.src_box = src_box_for_paste,
		.dst_box = mag_box,
		.filter_mode = rc.mag_filter ? WLR_SCALE_FILTER_BILINEAR
			: WLR_SCALE_FILTER_NEAREST,
	};
	wlr_render_pass_add_texture(tmp_render_pass, &opts);
	if (!wlr_render_pass_submit(tmp_render_pass)) {
		wlr_log(WLR_ERROR, "Failed to submit magnifier render pass");
		goto cleanup;
	}

	/* And finally mark the extra damage */
	*damage = damage_box;
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

static void
enable_magnifier(struct server *server, bool enable)
{
	magnify_on = enable;
	server->scene->direct_scanout = enable ? false
		: server->direct_scanout_enabled;
}

/* Toggles magnification on and off */
void
magnifier_toggle(struct server *server)
{
	enable_magnifier(server, !magnify_on);

	struct output *output = output_nearest_to_cursor(server);
	if (output) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

/* Increases and decreases magnification scale */
void
magnifier_set_scale(struct server *server, enum magnify_dir dir)
{
	struct output *output = output_nearest_to_cursor(server);

	if (dir == MAGNIFY_INCREASE) {
		if (magnify_on) {
			mag_scale += rc.mag_increment;
		} else {
			enable_magnifier(server, true);
			mag_scale = 1.0 + rc.mag_increment;
		}
	} else {
		if (magnify_on && mag_scale > 1.0 + rc.mag_increment) {
			mag_scale -= rc.mag_increment;
		} else {
			enable_magnifier(server, false);
		}
	}

	if (output) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

/* Reset any buffers held by the magnifier */
void
magnifier_reset(void)
{
	if (tmp_texture && tmp_buffer) {
		wlr_texture_destroy(tmp_texture);
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
		tmp_texture = NULL;
	}
}

/* Report whether magnification is enabled */
bool
magnifier_is_enabled(void)
{
	return magnify_on;
}
