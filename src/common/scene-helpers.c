// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "common/scene-helpers.h"
#include "labwc.h"

#include <wlr/render/pass.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/drm_format_set.h>
#include "common/macros.h"

static bool magnify_on;

struct wlr_surface *
lab_wlr_surface_from_node(struct wlr_scene_node *node)
{
	struct wlr_scene_buffer *buffer;
	struct wlr_scene_surface *scene_surface;

	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		buffer = wlr_scene_buffer_from_node(node);
		scene_surface = wlr_scene_surface_try_from_buffer(buffer);
		if (scene_surface) {
			return scene_surface->surface;
		}
	}
	return NULL;
}

struct wlr_scene_node *
lab_wlr_scene_get_prev_node(struct wlr_scene_node *node)
{
	assert(node);
	struct wlr_scene_node *prev;
	prev = wl_container_of(node->link.prev, node, link);
	if (&prev->link == &node->parent->children) {
		return NULL;
	}
	return prev;
}


/* TODO: move to rc. (or theme?) settings */
#define magnifier_border 1      /* in pixels */

static void
magnify(struct output *output, struct wlr_buffer *output_buffer, struct wlr_box *damage)
{
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
	struct wlr_cursor *cursor = server->seat.cursor;
	double ox = cursor->x;
	double oy = cursor->y;
	wlr_output_layout_output_coords(server->output_layout, output->wlr_output, &ox, &oy);
	ox *= output->wlr_output->scale;
	oy *= output->wlr_output->scale;

	/* TODO: refactor, to use rc. settings */
	int width = rc.mag_size + 1;
	int height = width;
	double x = ox - (rc.mag_size / 2.0);
	double y = oy - (rc.mag_size / 2.0);
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
		wlr_log(WLR_ERROR, "tmp buffer size changed, dropping");
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

	/* Extract source region into temporary buffer */

	struct wlr_render_pass *tmp_render_pass = wlr_renderer_begin_buffer_pass(
		server->renderer, tmp_buffer, NULL);

	/* FIXME, try to re-use the existing output texture instead */
	wlr_buffer_lock(output_buffer);
	struct wlr_texture *output_texture = wlr_texture_from_buffer(
		server->renderer, output_buffer);
	assert(output_texture);

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
	struct wlr_box border_box = {
		.x = ox - (width / 2 + magnifier_border),
		.y = oy - (height / 2 + magnifier_border),
		.width = (width + magnifier_border * 2),
		.height = (height + magnifier_border * 2),
	};
	struct wlr_render_rect_options bg_opts = {
		.box = border_box,
		/* TODO: make this a rc. setting */
		.color = (struct wlr_render_color) { 1, 0, 0, 1 },
		.clip = NULL,
	};
	wlr_render_pass_add_rect(tmp_render_pass, &bg_opts);

	/* Paste the magnified result back into the output buffer */
	if (!tmp_texture) {
		tmp_texture = wlr_texture_from_buffer(server->renderer, tmp_buffer);
		assert(tmp_texture);
	}
	opts = (struct wlr_render_texture_options) {
		.texture = tmp_texture,
		.src_box = (struct wlr_fbox) {
			.x = width * (rc.mag_scale - 1) / (2 * rc.mag_scale),
			.y = height * (rc.mag_scale - 1) / (2 * rc.mag_scale),
			.width = width / rc.mag_scale,
			.height = height / rc.mag_scale,
		},
		.dst_box = (struct wlr_box) {
			.x = ox - (width / 2),
			.y = oy - (height / 2),
			.width = width,
			.height = height,
		},
		.alpha = NULL,
		.clip = NULL,
		//.filter_mode = WLR_SCALE_FILTER_NEAREST,
		.filter_mode = WLR_SCALE_FILTER_BILINEAR,
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

static bool
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

void magnify_toggle (void)
{
	if (magnify_on) {
		magnify_on = false;
	} else {
		magnify_on = true;
	}
}

/*
 * Increases and decreases magnification scale
 */

void magnify_set_scale (enum magnify_dir dir)
{
	if (dir == MAGNIFY_INCREASE) {
		if (magnify_on) {
			rc.mag_scale++;
		} else {
			magnify_on = true;
			rc.mag_scale = 2;
		}
	} else {
		if (magnify_on && rc.mag_scale > 2) {
			rc.mag_scale--;
		} else {
			magnify_on = false;
		}
	}
}

/*
 * This is a copy of wlr_scene_output_commit()
 * as it doesn't use the pending state at all.
 */
bool
lab_wlr_scene_output_commit(struct wlr_scene_output *scene_output)
{
	assert(scene_output);
	struct wlr_output *wlr_output = scene_output->output;
	struct wlr_output_state *state = &wlr_output->pending;
	struct output *output = wlr_output->data;
	bool wants_magnification = output_wants_magnification(output);
	static bool last_mag = false;
	static int last_scale = 2;

	if (!wlr_output->needs_frame && !pixman_region32_not_empty(
			&scene_output->damage_ring.current) && !wants_magnification
			&& last_mag != magnify_on && last_scale != rc.mag_scale) {
		return false;
	}

	last_mag = magnify_on;
	last_scale = rc.mag_scale;

	if (!wlr_scene_output_build_state(scene_output, state, NULL)) {
		wlr_log(WLR_ERROR, "Failed to build output state for %s",
			wlr_output->name);
		return false;
	}

	struct wlr_box additional_damage = {0};
	if (state->buffer && magnify_on) {
		magnify(output, state->buffer, &additional_damage);
	}

	if (!wlr_output_commit(wlr_output)) {
		wlr_log(WLR_INFO, "Failed to commit output %s",
			wlr_output->name);
		return false;
	}
	/*
	 * FIXME: Remove the following line as soon as
	 * https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4253
	 * is merged. At that point wlr_scene handles damage tracking internally
	 * again.
	 */
	wlr_damage_ring_rotate(&scene_output->damage_ring);

	if (!wlr_box_empty(&additional_damage)) {
		wlr_damage_ring_add_box(&scene_output->damage_ring, &additional_damage);
	}
	return true;
}
