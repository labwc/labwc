// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "scaled-buffer/scaled-icon-buffer.h"
#include <assert.h>
#include <string.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "config.h"
#include "config/rcxml.h"
#include "desktop-entry.h"
#include "img/img.h"
#include "node.h"
#include "scaled-buffer/scaled-buffer.h"
#include "view.h"
#include "window-rules.h"

#if HAVE_LIBSFDO

static struct lab_data_buffer *
choose_best_icon_buffer(struct scaled_icon_buffer *self, int icon_size, double scale)
{
	int best_dist = -INT_MAX;
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

static struct lab_data_buffer *
img_to_buffer(struct lab_img *img, int width, int height, double scale)
{
	struct lab_data_buffer *buffer = lab_img_render(img, width, height, scale);
	lab_img_destroy(img);
	return buffer;
}

/*
 * Load an icon from application-supplied icon name or buffers.
 * Wayland apps can provide icon names and buffers via xdg-toplevel-icon protocol.
 * X11 apps can provide icon buffers via _NET_WM_ICON property.
 */
static struct lab_data_buffer *
load_client_icon(struct scaled_icon_buffer *self, int icon_size, double scale)
{
	struct lab_img *img = desktop_entry_load_icon(self->server,
		self->view_icon_name, icon_size, scale);
	if (img) {
		wlr_log(WLR_DEBUG, "loaded icon from client icon name");
		return img_to_buffer(img, self->width, self->height, scale);
	}

	struct lab_data_buffer *buffer = choose_best_icon_buffer(self, icon_size, scale);
	if (buffer) {
		wlr_log(WLR_DEBUG, "loaded icon from client buffer");
		return buffer_resize(buffer, self->width, self->height, scale);
	}

	return NULL;
}

/*
 * Load an icon by a view's app_id. For example, if the app_id is 'firefox', then
 * libsfdo will parse firefox.desktop to get the Icon name and then find that icon
 * based on the icon theme specified in rc.xml.
 */
static struct lab_data_buffer *
load_server_icon(struct scaled_icon_buffer *self, int icon_size, double scale)
{
	struct lab_img *img = desktop_entry_load_icon_from_app_id(self->server,
		self->view_app_id, icon_size, scale);
	if (img) {
		wlr_log(WLR_DEBUG, "loaded icon by app_id");
		return img_to_buffer(img, self->width, self->height, scale);
	}

	return NULL;
}

#endif /* HAVE_LIBSFDO */

static struct lab_data_buffer *
_create_buffer(struct scaled_buffer *scaled_buffer, double scale)
{
#if HAVE_LIBSFDO
	struct scaled_icon_buffer *self = scaled_buffer->data;
	int icon_size = MIN(self->width, self->height);
	struct lab_img *img = NULL;
	struct lab_data_buffer *buffer = NULL;

	if (self->icon_name) {
		/* generic icon (e.g. menu icons) */
		img = desktop_entry_load_icon(self->server, self->icon_name,
			icon_size, scale);
		if (img) {
			wlr_log(WLR_DEBUG, "loaded icon by icon name");
			return img_to_buffer(img, self->width, self->height, scale);
		}
		return NULL;
	}

	/* window icon */
	if (self->view_icon_prefer_client) {
		buffer = load_client_icon(self, icon_size, scale);
		if (buffer) {
			return buffer;
		}
		buffer = load_server_icon(self, icon_size, scale);
		if (buffer) {
			return buffer;
		}
	} else {
		buffer = load_server_icon(self, icon_size, scale);
		if (buffer) {
			return buffer;
		}
		buffer = load_client_icon(self, icon_size, scale);
		if (buffer) {
			return buffer;
		}
	}
	/* If both client and server icons are unavailable, use the fallback icon */
	img = desktop_entry_load_icon(self->server, rc.fallback_app_icon_name,
		icon_size, scale);
	if (img) {
		wlr_log(WLR_DEBUG, "loaded fallback icon");
		return img_to_buffer(img, self->width, self->height, scale);
	}
#endif /* HAVE_LIBSFDO */
	return NULL;
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
_destroy(struct scaled_buffer *scaled_buffer)
{
	struct scaled_icon_buffer *self = scaled_buffer->data;
	if (self->view) {
		wl_list_remove(&self->on_view.set_icon.link);
		wl_list_remove(&self->on_view.new_title.link);
		wl_list_remove(&self->on_view.new_app_id.link);
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
_equal(struct scaled_buffer *scaled_buffer_a,
	struct scaled_buffer *scaled_buffer_b)
{
	struct scaled_icon_buffer *a = scaled_buffer_a->data;
	struct scaled_icon_buffer *b = scaled_buffer_b->data;

	return str_equal(a->view_app_id, b->view_app_id)
		&& a->view_icon_prefer_client == b->view_icon_prefer_client
		&& str_equal(a->view_icon_name, b->view_icon_name)
		&& icon_buffers_equal(&a->view_icon_buffers, &b->view_icon_buffers)
		&& str_equal(a->icon_name, b->icon_name)
		&& a->width == b->width
		&& a->height == b->height;
}

static struct scaled_buffer_impl impl = {
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

	struct scaled_buffer *scaled_buffer = scaled_buffer_create(
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

	bool icon_name_equal = str_equal(self->view_icon_name, self->view->icon.name);
	if (icon_name_equal && icon_buffers_equal(&self->view_icon_buffers,
			&self->view->icon.buffers)) {
		return;
	}

	if (!icon_name_equal) {
		zfree(self->view_icon_name);
		if (self->view->icon.name) {
			xstrdup_replace(self->view_icon_name, self->view->icon.name);
		}
	}

	set_icon_buffers(self, &self->view->icon.buffers);
	scaled_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

static void
handle_view_new_title(struct wl_listener *listener, void *data)
{
	struct scaled_icon_buffer *self =
		wl_container_of(listener, self, on_view.new_title);

	bool prefer_client = window_rules_get_property(
		self->view, "iconPreferClient") == LAB_PROP_TRUE;
	if (prefer_client == self->view_icon_prefer_client) {
		return;
	}
	self->view_icon_prefer_client = prefer_client;
	scaled_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

static void
handle_view_new_app_id(struct wl_listener *listener, void *data)
{
	struct scaled_icon_buffer *self =
		wl_container_of(listener, self, on_view.new_app_id);

	const char *app_id = self->view->app_id;
	if (str_equal(app_id, self->view_app_id)) {
		return;
	}

	xstrdup_replace(self->view_app_id, app_id);
	self->view_icon_prefer_client = window_rules_get_property(
		self->view, "iconPreferClient") == LAB_PROP_TRUE;
	scaled_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

static void
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct scaled_icon_buffer *self =
		wl_container_of(listener, self, on_view.destroy);
	wl_list_remove(&self->on_view.destroy.link);
	wl_list_remove(&self->on_view.set_icon.link);
	wl_list_remove(&self->on_view.new_title.link);
	wl_list_remove(&self->on_view.new_app_id.link);
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
		wl_list_remove(&self->on_view.new_title.link);
		wl_list_remove(&self->on_view.new_app_id.link);
		wl_list_remove(&self->on_view.destroy.link);
	}
	self->view = view;

	self->on_view.set_icon.notify = handle_view_set_icon;
	wl_signal_add(&view->events.set_icon, &self->on_view.set_icon);

	self->on_view.new_title.notify = handle_view_new_title;
	wl_signal_add(&view->events.new_title, &self->on_view.new_title);

	self->on_view.new_app_id.notify = handle_view_new_app_id;
	wl_signal_add(&view->events.new_app_id, &self->on_view.new_app_id);

	self->on_view.destroy.notify = handle_view_destroy;
	wl_signal_add(&view->events.destroy, &self->on_view.destroy);

	handle_view_set_icon(&self->on_view.set_icon, NULL);
	handle_view_new_app_id(&self->on_view.new_app_id, NULL);
	handle_view_new_title(&self->on_view.new_title, NULL);
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
	scaled_buffer_request_update(self->scaled_buffer, self->width, self->height);
}
