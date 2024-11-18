/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCALED_SCENE_BUFFER_H
#define LABWC_SCALED_SCENE_BUFFER_H

#include <wayland-server-core.h>

#define LAB_SCALED_BUFFER_MAX_CACHE 2

struct wlr_buffer;
struct wlr_scene_tree;
struct lab_data_buffer;
struct scaled_scene_buffer;

struct scaled_scene_buffer_impl {
	/* Return a new buffer optimized for the new scale */
	struct lab_data_buffer *(*create_buffer)
		(struct scaled_scene_buffer *scaled_buffer, double scale);
	/* Might be NULL or used for cleaning up */
	void (*destroy)(struct scaled_scene_buffer *scaled_buffer);
	/* Returns true if the two buffers are visually the same */
	bool (*equal)(struct scaled_scene_buffer *scaled_buffer_a,
		struct scaled_scene_buffer *scaled_buffer_b);
};

struct scaled_scene_buffer {
	struct wlr_scene_buffer *scene_buffer;
	int width;   /* unscaled, read only */
	int height;  /* unscaled, read only */
	void *data;  /* opaque user data */

	/* Private */
	bool drop_buffer;
	double active_scale;
	/* cached wlr_buffers for each scale */
	struct wl_list cache;  /* struct scaled_buffer_cache_entry.link */
	struct wl_listener destroy;
	struct wl_listener output_enter;
	struct wl_listener output_leave;
	const struct scaled_scene_buffer_impl *impl;
	/*
	 * Pointer to the per-implementation list of scaled-scene-buffers.
	 * This is used to share the backing wlr_buffers.
	 */
	struct wl_list *cached_buffers;
	struct wl_list link; /* struct scaled_scene_buffer.cached_buffers */
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
 *
 * Besides caching buffers for each scale per scaled_scene_buffer, we also
 * store all the scaled_scene_buffers in a per-implementer list passed as
 * @cached_buffers in order to reuse backing buffers for visually duplicated
 * scaled_scene_buffers found via impl->equal().
 *
 * All requested lab_data_buffers via impl->create_buffer() will be locked
 * during the lifetime of the buffer in the internal cache and unlocked
 * when being evacuated from the cache (due to LAB_SCALED_BUFFER_MAX_CACHE
 * or the internal wlr_scene_buffer being destroyed).
 *
 * If drop_buffer was set during creation of the scaled_scene_buffer, the
 * backing wlr_buffer behind a lab_data_buffer will also get dropped
 * (via wlr_buffer_drop). If there are no more locks (consumers) of the
 * respective buffer this will then cause the lab_data_buffer to be free'd.
 *
 * In the case of the buffer provider dropping the buffer itself (due to
 * for example a Reconfigure event) the lock prevents the buffer from being
 * destroyed until the buffer is evacuated from the internal cache and thus
 * unlocked.
 *
 * This allows using scaled_scene_buffer for an autoscaling font_buffer
 * (which gets free'd automatically) and also for theme components like
 * rounded corner images or button icons whose buffers only exist once but
 * are references by multiple windows with their own scaled_scene_buffers.
 *
 * The rough idea is: use drop_buffer = true for one-shot buffers and false
 * for buffers that should outlive the scaled_scene_buffer instance itself.
 */
struct scaled_scene_buffer *scaled_scene_buffer_create(
	struct wlr_scene_tree *parent,
	const struct scaled_scene_buffer_impl *implementation,
	struct wl_list *cached_buffers, bool drop_buffer);

/* Clear the cache of existing buffers, useful in case the content changes */
void scaled_scene_buffer_invalidate_cache(struct scaled_scene_buffer *self);

/* Private */
struct scaled_scene_buffer_cache_entry {
	struct wl_list link;   /* struct scaled_scene_buffer.cache */
	struct wlr_buffer *buffer;
	double scale;
};

#endif /* LABWC_SCALED_SCENE_BUFFER_H */
