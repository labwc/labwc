// SPDX-License-Identifier: GPL-2.0-only

#include "animation.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include "common/mem.h"
#include "output.h"
#include "view.h"

#define ANIMATION_DURATION 0.2 /* s */

struct animation {
	struct wlr_scene_transform *transform;
	struct wlr_output *wlr_output;
	uint64_t start_time;

	struct wl_listener output_commit;
	struct wl_listener output_destroy;
	struct wl_listener view_destroy;
};

static inline uint64_t
timespec_to_us(const struct timespec *ts)
{
	return (uint64_t)ts->tv_sec * UINT64_C(1000000) +
		(uint64_t)ts->tv_nsec / UINT64_C(1000);
}

static inline uint64_t
gettime_us(void)
{
	struct timespec ts = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return timespec_to_us(&ts);
}

static void
animation_destroy(struct animation *animation)
{
	wlr_scene_transform_set_dst_box(animation->transform,
		&(struct wlr_box){0});

	wl_list_remove(&animation->output_commit.link);
	wl_list_remove(&animation->output_destroy.link);
	wl_list_remove(&animation->view_destroy.link);
	free(animation);
}

static void
animation_set_scale(struct animation *animation, double scale)
{
	/* The box starts out empty when there's no buffer set */
	int width = 1, height = 1;
	struct wlr_box src;
	wlr_scene_node_get_source_box(&animation->transform->node, &src);
	if (!wlr_box_empty(&src)) {
		/* Empty dst_box disables the transform */
		width = fmax(round(src.width * scale), 1);
		height = fmax(round(src.height * scale), 1);
	}

	struct wlr_box dst = {
		.x = src.x + (src.width - width) / 2,
		.y = src.y + (src.height - height) / 2,
		.width = width,
		.height = height,
	};
	wlr_scene_transform_set_dst_box(animation->transform, &dst);
}

/* Returns false when the animation has finished */
static bool
animation_run(struct animation *animation, uint64_t now)
{
	uint64_t elapsed_us = now - animation->start_time;
	double t = (double)elapsed_us * 1e-6;
	double scale = fmin(t / ANIMATION_DURATION, 1.0);

	animation_set_scale(animation, scale);

	return scale < 1.0;
}

static void
handle_output_commit(struct wl_listener *listener, void *data)
{
	struct animation *animation =
		wl_container_of(listener, animation, output_commit);
	struct wlr_output_event_commit *event = data;

	if (!(event->state->committed & WLR_OUTPUT_STATE_BUFFER)) {
		return;
	}

	if (!animation_run(animation, gettime_us())) {
		animation_destroy(animation);
		return;
	}

	wlr_output_schedule_frame(event->output);
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct animation *animation =
		wl_container_of(listener, animation, output_destroy);
	animation_destroy(animation);
}

static void
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct animation *animation =
		wl_container_of(listener, animation, view_destroy);
	animation_destroy(animation);
}

void
animation_fade_in(struct view *view)
{
	if (!view->output || !view->output->wlr_output) {
		return;
	}

	struct animation *animation = znew(*animation);
	animation->transform =
		wlr_scene_transform_from_node(&view->scene_tree->node);
	animation->wlr_output = view->output->wlr_output;

	animation->output_commit.notify = handle_output_commit;
	wl_signal_add(&animation->wlr_output->events.commit,
		&animation->output_commit);

	animation->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&animation->wlr_output->events.destroy,
		&animation->output_destroy);

	animation->view_destroy.notify = handle_view_destroy;
	wl_signal_add(&view->events.destroy, &animation->view_destroy);

	uint64_t now = gettime_us();
	animation->start_time = now;
	animation_run(animation, now);
	wlr_output_schedule_frame(animation->wlr_output);
}
