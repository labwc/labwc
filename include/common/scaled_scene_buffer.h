/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LAB_COMMON_SCALED_SCENE_BUFFER_H
#define __LAB_COMMON_SCALED_SCENE_BUFFER_H

#define LAB_SCALED_BUFFER_MAX_CACHE 2

struct wl_list;
struct wlr_buffer;
struct wl_listener;
struct wlr_scene_tree;
struct lab_data_buffer;
struct scaled_scene_buffer;

struct scaled_scene_buffer_impl {
	/* Return a new buffer optimized for the new scale */
	struct lab_data_buffer *(*create_buffer)
		(struct scaled_scene_buffer *scaled_buffer, double scale);
	/* Might be NULL or used for cleaning up */
	void (*destroy)(struct scaled_scene_buffer *scaled_buffer);
};

struct scaled_scene_buffer {
	struct wlr_scene_buffer *scene_buffer;
	int width;   /* unscaled, read only */
	int height;  /* unscaled, read only */
	void *data;  /* opaque user data */

	/* Private */
	double active_scale;
	struct wl_list cache;  /* struct scaled_buffer_cache_entry.link */
	struct wl_listener destroy;
	struct wl_listener output_enter;
	struct wl_listener output_leave;
	const struct scaled_scene_buffer_impl *impl;
};

/**
 * Create an auto scaling buffer that creates a wlr_scene_buffer
 * and subscribes to its output_enter and output_leave signals.
 *
 * If the maximal scale changes, it either sets an already existing buffer
 * that was rendered for the current scale or - if there is none - calls
 * implementation->create_buffer(self, scale) to get a new lab_data_buffer
 * optimized for the new scale.
 *
 * Up to LAB_SCALED_BUFFER_MAX_CACHE (2) buffers are cached in an LRU fashion
 * to handle the majority of use cases where a view is moved between no more
 * than two different scales.
 *
 * scaled_scene_buffer will clean up automatically once the internal
 * wlr_scene_buffer is being destroyed. If implementation->destroy is set
 * it will also get called so a consumer of this API may clean up its own
 * allocations.
 */
struct scaled_scene_buffer *scaled_scene_buffer_create(
	struct wlr_scene_tree *parent,
	const struct scaled_scene_buffer_impl *implementation);

/* Clear the cache of existing buffers, useful in case the content changes */
void scaled_scene_buffer_invalidate_cache(struct scaled_scene_buffer *self);

/* Private */
struct scaled_scene_buffer_cache_entry {
	struct wl_list link;   /* struct scaled_scene_buffer.cache */
	struct wlr_buffer *buffer;
	double scale;
};

#endif /* __LAB_COMMON_SCALED_SCENE_BUFFER_H */
