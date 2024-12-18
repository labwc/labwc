// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/mem.h"
#include "common/scaled-scene-buffer.h"
#include "node.h"

/**
 * TODO
 *
 * This implementation does not react to output scale changes itself but only
 * to movement of scene nodes to different outputs. To also handle output scale
 * changes we'd need to keep a list of struct wlr_outputs * in sync and listen
 * to their commit signals.
 *
 * The detection of the max output scale is also not 100% robust for all use
 * cases as on output_enter we only compare the new output scale with the
 * primary_output scale. In specific conditions the primary output may be the
 * same as the new output even though a smaller part of the buffer node is
 * still visible on an output with a bigger scale. This could be solved the
 * same way as the previous point in keeping a list of outputs in sync.
 *
 * Most of this would be easier when wlroots would instead provide a
 * max_scale_changed event because it already touches all the relevant parts
 * when calculating the primary_output and inform wlr_scene_buffers about
 * output changes.
 *
 * See wlroots/types/scene/wlr_scene.c scene_buffer_update_outputs()
 */

/* Internal API */
static void
_cache_entry_destroy(struct scaled_scene_buffer_cache_entry *cache_entry, bool drop_buffer)
{
	wl_list_remove(&cache_entry->link);
	if (cache_entry->buffer) {
		/* Allow the buffer to get dropped if there are no further consumers */
		if (drop_buffer && !cache_entry->buffer->dropped) {
			wlr_buffer_drop(cache_entry->buffer);
		}
		wlr_buffer_unlock(cache_entry->buffer);
	}
	free(cache_entry);
}

static struct scaled_scene_buffer_cache_entry *
find_cache_for_scale(struct scaled_scene_buffer *scene_buffer, double scale)
{
	struct scaled_scene_buffer_cache_entry *cache_entry;
	wl_list_for_each(cache_entry, &scene_buffer->cache, link) {
		if (cache_entry->scale == scale) {
			return cache_entry;
		}
	}
	return NULL;
}

static void
_update_buffer(struct scaled_scene_buffer *self, double scale)
{
	self->active_scale = scale;

	/* Search for cached buffer of specified scale */
	struct scaled_scene_buffer_cache_entry *cache_entry =
		find_cache_for_scale(self, scale);
	if (cache_entry) {
		/* LRU cache, recently used in front */
		wl_list_remove(&cache_entry->link);
		wl_list_insert(&self->cache, &cache_entry->link);
		wlr_scene_buffer_set_buffer(self->scene_buffer, cache_entry->buffer);
		/*
		 * If found in our local cache,
		 * - self->width and self->height are already set
		 * - wlr_scene_buffer_set_dest_size() has already been called
		 */
		return;
	}

	struct wlr_buffer *wlr_buffer = NULL;

	if (self->impl->equal && self->cached_buffers) {
		/* Search from other cached scaled-scene-buffers */
		struct scaled_scene_buffer *scene_buffer;
		wl_list_for_each(scene_buffer, self->cached_buffers, link) {
			if (scene_buffer == self) {
				continue;
			}
			if (!self->impl->equal(self, scene_buffer)) {
				continue;
			}
			cache_entry = find_cache_for_scale(scene_buffer, scale);
			if (!cache_entry) {
				continue;
			}

			/* Ensure self->width and self->height are set correctly */
			self->width = scene_buffer->width;
			self->height = scene_buffer->height;
			wlr_buffer = cache_entry->buffer;
			break;
		}
	}

	if (!wlr_buffer) {
		/*
		 * Create new buffer, will get destroyed along the backing
		 * wlr_buffer
		 */
		struct lab_data_buffer *buffer =
			self->impl->create_buffer(self, scale);
		if (buffer) {
			self->width = buffer->logical_width;
			self->height = buffer->logical_height;
			wlr_buffer = &buffer->base;
		} else {
			self->width = 0;
			self->height = 0;
		}
	}
	if (wlr_buffer) {
		/* Ensure the buffer doesn't get deleted behind our back */
		wlr_buffer_lock(wlr_buffer);
	}

	/* Create or reuse cache entry */
	if (wl_list_length(&self->cache) < LAB_SCALED_BUFFER_MAX_CACHE) {
		cache_entry = znew(*cache_entry);
	} else {
		cache_entry = wl_container_of(self->cache.prev, cache_entry, link);
		if (cache_entry->buffer) {
			/* Allow the old buffer to get dropped if there are no further consumers */
			if (self->drop_buffer && !cache_entry->buffer->dropped) {
				wlr_buffer_drop(cache_entry->buffer);
			}
			wlr_buffer_unlock(cache_entry->buffer);
		}
		wl_list_remove(&cache_entry->link);
	}

	/* Update the cache entry */
	cache_entry->scale = scale;
	cache_entry->buffer = wlr_buffer;
	wl_list_insert(&self->cache, &cache_entry->link);

	/* And finally update the wlr_scene_buffer itself */
	wlr_scene_buffer_set_buffer(self->scene_buffer, cache_entry->buffer);
	wlr_scene_buffer_set_dest_size(self->scene_buffer, self->width, self->height);
}

/* Internal event handlers */
static void
_handle_node_destroy(struct wl_listener *listener, void *data)
{
	struct scaled_scene_buffer_cache_entry *cache_entry, *cache_entry_tmp;
	struct scaled_scene_buffer *self = wl_container_of(listener, self, destroy);

	wl_list_remove(&self->destroy.link);
	wl_list_remove(&self->output_enter.link);
	wl_list_remove(&self->output_leave.link);

	wl_list_for_each_safe(cache_entry, cache_entry_tmp, &self->cache, link) {
		_cache_entry_destroy(cache_entry, self->drop_buffer);
	}
	assert(wl_list_empty(&self->cache));

	if (self->impl->destroy) {
		self->impl->destroy(self);
	}
	wl_list_remove(&self->link);
	free(self);
}

static void
_handle_output_enter(struct wl_listener *listener, void *data)
{
	struct scaled_scene_buffer *self =
		wl_container_of(listener, self, output_enter);
	/* primary_output is the output most of the node area is in */
	struct wlr_scene_output *primary = self->scene_buffer->primary_output;
	/* scene_output is the output we just entered */
	struct wlr_scene_output *scene_output = data;
	double max_scale = scene_output->output->scale;

	if (primary && primary->output->scale > max_scale) {
		max_scale = primary->output->scale;
	}

	if (self->active_scale != max_scale) {
		_update_buffer(self, max_scale);
	}
}

static void
_handle_output_leave(struct wl_listener *listener, void *data)
{
	struct scaled_scene_buffer *self =
		wl_container_of(listener, self, output_leave);
	/* primary_output is the output most of the node area is in */
	struct wlr_scene_output *primary = self->scene_buffer->primary_output;

	if (primary && primary->output->scale != self->active_scale) {
		_update_buffer(self, primary->output->scale);
	}
}

/* Public API */
struct scaled_scene_buffer *
scaled_scene_buffer_create(struct wlr_scene_tree *parent,
		const struct scaled_scene_buffer_impl *impl,
		struct wl_list *cached_buffers, bool drop_buffer)
{
	assert(parent);
	assert(impl);
	assert(impl->create_buffer);

	struct scaled_scene_buffer *self = znew(*self);
	self->scene_buffer = wlr_scene_buffer_create(parent, NULL);
	if (!self->scene_buffer) {
		wlr_log(WLR_ERROR, "Failed to create scene buffer");
		free(self);
		return NULL;
	}
	node_descriptor_create(&self->scene_buffer->node,
		LAB_NODE_DESC_SCALED_SCENE_BUFFER, self);

	self->impl = impl;
	/*
	 * Set active scale to zero so that we always render a new buffer when
	 * entering the first output
	 */
	self->active_scale = 0;
	self->drop_buffer = drop_buffer;
	wl_list_init(&self->cache);

	self->cached_buffers = cached_buffers;
	if (self->cached_buffers) {
		wl_list_insert(self->cached_buffers, &self->link);
	} else {
		/* Ensure self->link can be removed safely in the destroy handler */
		wl_list_init(&self->link);
	}

	/* Listen to output enter/leave so we get notified about scale changes */
	self->output_enter.notify = _handle_output_enter;
	wl_signal_add(&self->scene_buffer->events.output_enter, &self->output_enter);
	self->output_leave.notify = _handle_output_leave;
	wl_signal_add(&self->scene_buffer->events.output_leave, &self->output_leave);

	/* Let it destroy automatically when the scene node destroys */
	self->destroy.notify = _handle_node_destroy;
	wl_signal_add(&self->scene_buffer->node.events.destroy, &self->destroy);

	return self;
}

void
scaled_scene_buffer_request_update(struct scaled_scene_buffer *self,
		int width, int height)
{
	assert(self);
	struct scaled_scene_buffer_cache_entry *cache_entry, *cache_entry_tmp;
	wl_list_for_each_safe(cache_entry, cache_entry_tmp, &self->cache, link) {
		_cache_entry_destroy(cache_entry, self->drop_buffer);
	}
	assert(wl_list_empty(&self->cache));

	/*
	 * Tell wlroots about the buffer size so we can receive output_enter
	 * events even when the actual backing buffer is not set yet.
	 * The buffer size set here is updated when the backing buffer is
	 * created in _update_buffer().
	 */
	wlr_scene_buffer_set_dest_size(self->scene_buffer, width, height);
	self->width = width;
	self->height = height;

	/*
	 * Skip re-rendering if the buffer is not shown yet
	 * TODO: don't re-render also when the buffer is temporarily invisible
	 */
	if (self->active_scale > 0) {
		_update_buffer(self, self->active_scale);
	}
}
