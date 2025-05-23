// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include <wlr/util/log.h>
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

static struct lab_img *
load_icon_img(struct scaled_icon_buffer *self, double scale)
{
	int icon_size = MIN(self->width, self->height);

	if (self->icon_name) {
		return desktop_entry_load_icon(self->server,
			self->icon_name, icon_size, scale);
	} else if (self->view) {
		wlr_log(WLR_DEBUG, "loading icon by app_id");
		const char *app_id =
			view_get_string_prop(self->view, "app_id");
		struct lab_img *img = desktop_entry_load_icon_from_app_id(
			self->server, app_id, icon_size, scale);
		if (!img) {
			wlr_log(WLR_DEBUG, "loading fallback icon");
			img = desktop_entry_load_icon(self->server,
				rc.fallback_app_icon_name, icon_size, scale);
		}
		return img;
	}

	return NULL;
}

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
#if HAVE_LIBSFDO
	struct scaled_icon_buffer *self = scaled_buffer->data;

	struct lab_img *img = load_icon_img(self, scale);
	if (!img) {
		return NULL;
	}

	struct lab_data_buffer *buffer =
		lab_img_render(img, self->width, self->height, scale);
	lab_img_destroy(img);

	return buffer;
#else
	return NULL;
#endif
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_icon_buffer *self = scaled_buffer->data;
	if (self->view) {
		wl_list_remove(&self->view_destroy.link);
	}
	free(self->icon_name);
	free(self);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a,
	struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_icon_buffer *a = scaled_buffer_a->data;
	struct scaled_icon_buffer *b = scaled_buffer_b->data;

	return a->view == b->view
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
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct scaled_icon_buffer *self =
		wl_container_of(listener, self, view_destroy);
	wl_list_remove(&self->view_destroy.link);
	self->view = NULL;
}

void
scaled_icon_buffer_set_view(struct scaled_icon_buffer *self,
	struct view *view)
{
	assert(view);
	if (self->view == view) {
		return;
	}

	if (self->view) {
		wl_list_remove(&self->view_destroy.link);
	}
	self->view = view;
	if (view) {
		self->view_destroy.notify = handle_view_destroy;
		wl_signal_add(&view->events.destroy, &self->view_destroy);
	}

	scaled_scene_buffer_request_update(self->scaled_buffer, self->width, self->height);
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
