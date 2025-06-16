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
#include "window-rules.h"

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

struct icon_load_ctx {
	struct scaled_icon_buffer *self;
	const char *icon_name;
	int icon_size;
	double scale;
};

typedef struct lab_data_buffer *(*load_icon_func_t)(struct icon_load_ctx *ctx);

/*
 * Load an icon by name e.g. 'firefox'. libsfdo will search for the
 * icon (e.g. firefox.svg) using the icon theme specified in rc.xml.
 */
static struct lab_data_buffer *
load_icon_by_name(struct icon_load_ctx *ctx)
{
	if (!ctx->icon_name) {
		return NULL;
	}
	struct lab_img *img = desktop_entry_load_icon(
		ctx->self->server, ctx->icon_name, ctx->icon_size, ctx->scale);
	if (!img) {
		return NULL;
	}

	wlr_log(WLR_DEBUG, "loaded icon by name: %s", ctx->icon_name);
	struct lab_data_buffer *buffer = lab_img_render(
		img, ctx->self->width, ctx->self->height, ctx->scale);
	lab_img_destroy(img);
	return buffer;
}

static struct lab_data_buffer *
load_client_icon(struct icon_load_ctx *ctx)
{
	wlr_log(WLR_INFO, "Trying to load icon from client");
	ctx->icon_name = ctx->self->view_icon_name;
	struct lab_data_buffer *buffer = load_icon_by_name(ctx);
	if (buffer) {
		return buffer;
	}

	buffer = choose_best_icon_buffer(ctx->self, ctx->icon_size, ctx->scale);
	if (!buffer) {
		return NULL;
	}

	wlr_log(WLR_DEBUG, "loaded icon from client buffer");
	return buffer_resize(buffer,
		ctx->self->width, ctx->self->height, ctx->scale);
}

/*
 * Load an icon by a view's app_id. For example, if the app_id is 'firefox', then
 * libsfdo will parse firefox.desktop to get the Icon name and then find that icon
 * based on the icon theme specified in rc.xml.
 */
static struct lab_data_buffer *
load_icon_by_app_id(struct icon_load_ctx *ctx)
{
	wlr_log(WLR_INFO, "Trying to load icon via app id");
	struct lab_img *img = desktop_entry_load_icon_from_app_id(
		ctx->self->server, ctx->self->view_app_id, ctx->icon_size, ctx->scale);
	if (!img) {
		return NULL;
	}

	wlr_log(WLR_DEBUG, "loaded icon by app_id");
	struct lab_data_buffer *buffer = lab_img_render(
		img, ctx->self->width, ctx->self->height, ctx->scale);
	lab_img_destroy(img);
	return buffer;
}

#endif /* HAVE_LIBSFDO */

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
#if HAVE_LIBSFDO
	struct scaled_icon_buffer *self = scaled_buffer->data;
	int icon_size = MIN(self->width, self->height);

	struct {
		load_icon_func_t fn;
		const char *icon_name;
	} load_methods[] = {
		/* First try to load a specified icon as used by menu.c */
		{ load_icon_by_name, self->icon_name },
		/* Following two are set below */
		{ NULL, NULL },
		{ NULL, NULL },
		{ load_icon_by_name, rc.fallback_app_icon_name },
	};
	if (self->view) {
		if (window_rules_get_property(self->view, "iconPreferServer") == LAB_PROP_TRUE) {
			load_methods[1].fn = load_icon_by_app_id;
			load_methods[2].fn = load_client_icon;
		} else {
			load_methods[1].fn = load_client_icon;
			load_methods[2].fn = load_icon_by_app_id;
		}
	}
	struct lab_data_buffer *buffer = NULL;
	struct icon_load_ctx ctx = { self, NULL, icon_size, scale };
	for (size_t i = 0; i < ARRAY_SIZE(load_methods); i++) {
		if (!load_methods[i].fn) {
			continue;
		}
		ctx.icon_name = load_methods[i].icon_name;
		buffer = load_methods[i].fn(&ctx);
		if (buffer) {
			return buffer;
		}
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
