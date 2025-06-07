// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/scaled-icon-buffer.h"
#include "common/scaled-scene-buffer.h"
#include "common/string-helpers.h"
#include "config.h"
#include "config/rcxml.h"
#include "desktop-entry.h"
#include "img/img.h"
#include "node.h"
#include "view.h"

#if HAVE_LIBSFDO

static struct lab_data_buffer *
choose_best_icon_buffer(struct scaled_icon_buffer *self, int icon_size, double scale)
{
	int best_dist = INT_MIN;
	struct lab_data_buffer *best_buffer = NULL;

	struct lab_data_buffer **buffer;
	wl_array_for_each(buffer, &self->view_icon_buffers) {
		int curr_dist = (*buffer)->base.width - (int)(icon_size * scale);
		bool curr_is_better;
		if ((curr_dist < 0 && best_dist > 0)
				|| (curr_dist > 0 && best_dist < 0)) {
			/* prefer too big icon over too small icon */
			curr_is_better = curr_dist > 0;
		} else {
			curr_is_better = abs(curr_dist) < abs(best_dist);
		}
		if (curr_is_better) {
			best_dist = curr_dist;
			best_buffer = *buffer;
		}
	}
	return best_buffer;
}
#endif /* HAVE_LIBSFDO */

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
#if HAVE_LIBSFDO
	struct scaled_icon_buffer *self = scaled_buffer->data;
	int icon_size = MIN(self->width, self->height);
	struct lab_img *img = NULL;

	if (self->icon_name) {
		img = desktop_entry_load_icon(self->server,
			self->icon_name, icon_size, scale);
	} else if (self->view) {
		if (self->view_icon_name) {
			wlr_log(WLR_DEBUG, "loading icon by name: %s",
				self->view_icon_name);
			img = desktop_entry_load_icon(self->server,
				self->view_icon_name, icon_size, scale);
		}
		if (!img) {
			struct lab_data_buffer *buffer =
				choose_best_icon_buffer(self, icon_size, scale);
			if (buffer) {
				wlr_log(WLR_DEBUG, "loading icon by buffer");
				return buffer_resize(buffer,
					self->width, self->height, scale);
			}
		}
		if (!img) {
			wlr_log(WLR_DEBUG, "loading icon by app_id");
			img = desktop_entry_load_icon_from_app_id(self->server,
				self->view_app_id, icon_size, scale);
		}
		if (!img) {
			wlr_log(WLR_DEBUG, "loading fallback icon");
			img = desktop_entry_load_icon(self->server,
				rc.fallback_app_icon_name, icon_size, scale);
		}
	}

	if (!img) {
		return NULL;
	}

	struct lab_data_buffer *buffer =
		lab_img_render(img, self->width, self->height, scale);
	lab_img_destroy(img);

	return buffer;
#else
	return NULL;
#endif /* HAVE_LIBSFDO */
}

static void
set_icon_buffers(struct scaled_icon_buffer *self, struct wl_array *buffers)
{
	struct lab_data_buffer **icon_buffer;
	wl_array_for_each(icon_buffer, &self->view_icon_buffers) {
		wlr_buffer_unlock(&(*icon_buffer)->base);
	}
	wl_array_release(&self->view_icon_buffers);
	wl_array_init(&self->view_icon_buffers);

	if (!buffers) {
		return;
	}

	wl_array_for_each(icon_buffer, buffers) {
		wlr_buffer_lock(&(*icon_buffer)->base);
	}
	wl_array_copy(&self->view_icon_buffers, buffers);
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_icon_buffer *self = scaled_buffer->data;
	if (self->view) {
		wl_list_remove(&self->on_view.set_icon.link);
		wl_list_remove(&self->on_view.destroy.link);
	}
	free(self->view_app_id);
	free(self->view_icon_name);
	set_icon_buffers(self, NULL);
	free(self->icon_name);
	free(self);
}

static bool
icon_buffers_equal(struct wl_array *a, struct wl_array *b)
{
	if (a->size != b->size) {
		return false;
	}
	return a->size == 0 || !memcmp(a->data, b->data, a->size);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a,
	struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_icon_buffer *a = scaled_buffer_a->data;
	struct scaled_icon_buffer *b = scaled_buffer_b->data;

	return str_equal(a->view_app_id, b->view_app_id)
		&& str_equal(a->view_icon_name, b->view_icon_name)
		&& icon_buffers_equal(&a->view_icon_buffers, &b->view_icon_buffers)
		&& str_equal(a->icon_name, b->icon_name)
		&& a->width == b->width
		&& a->height == b->height;
}

static struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

struct scaled_icon_buffer *
scaled_icon_buffer_create(struct wlr_scene_tree *parent, struct server *server,
	int width, int height)
{
	assert(parent);
	assert(width >= 0 && height >= 0);

	struct scaled_scene_buffer *scaled_buffer = scaled_scene_buffer_create(
		parent, &impl, /* drop_buffer */ true);
	struct scaled_icon_buffer *self = znew(*self);
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	self->server = server;
	self->width = width;
	self->height = height;

	scaled_buffer->data = self;

	return self;
}

static void
handle_view_set_icon(struct wl_listener *listener, void *data)
{
	struct scaled_icon_buffer *self =
		wl_container_of(listener, self, on_view.set_icon);

	/* view_get_string_prop() never returns NULL */
	xstrdup_replace(self->view_app_id, view_get_string_prop(self->view, "app_id"));
	zfree(self->view_icon_name);
	if (self->view->icon.name) {
		self->view_icon_name = xstrdup(self->view->icon.name);
	}
	set_icon_buffers(self, &self->view->icon.buffers);

	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

static void
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct scaled_icon_buffer *self =
		wl_container_of(listener, self, on_view.destroy);
	wl_list_remove(&self->on_view.destroy.link);
	wl_list_remove(&self->on_view.set_icon.link);
	self->view = NULL;
}

void
scaled_icon_buffer_set_view(struct scaled_icon_buffer *self, struct view *view)
{
	assert(view);
	if (self->view == view) {
		return;
	}

	if (self->view) {
		wl_list_remove(&self->on_view.set_icon.link);
		wl_list_remove(&self->on_view.destroy.link);
	}
	self->view = view;
	self->on_view.set_icon.notify = handle_view_set_icon;
	wl_signal_add(&view->events.set_icon, &self->on_view.set_icon);
	self->on_view.destroy.notify = handle_view_destroy;
	wl_signal_add(&view->events.destroy, &self->on_view.destroy);

	handle_view_set_icon(&self->on_view.set_icon, NULL);
}

void
scaled_icon_buffer_set_icon_name(struct scaled_icon_buffer *self,
	const char *icon_name)
{
	assert(icon_name);
	if (str_equal(self->icon_name, icon_name)) {
		return;
	}
	xstrdup_replace(self->icon_name, icon_name);
	scaled_scene_buffer_request_update(self->scaled_buffer, self->width, self->height);
}

struct scaled_icon_buffer *
scaled_icon_buffer_from_node(struct wlr_scene_node *node)
{
	struct scaled_scene_buffer *scaled_buffer =
		node_scaled_scene_buffer_from_node(node);
	assert(scaled_buffer->impl == &impl);
	return scaled_buffer->data;
}
