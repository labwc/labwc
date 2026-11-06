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

static inline int64_t
timespec_to_us(const struct timespec *ts)
{
	return (int64_t)ts->tv_sec * INT64_C(1000000) +
		(int64_t)ts->tv_nsec / INT64_C(1000);
}

static inline int64_t
gettime_us(void)
{
	struct timespec ts = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return timespec_to_us(&ts);
}

static void
animation_destroy(struct animation *animation)
{
	wl_list_remove(&animation->output_commit.link);
	wl_list_remove(&animation->output_destroy.link);
	wl_list_remove(&animation->view_destroy.link);
	zfree(animation);
}

void
animation_finish(struct animation *animation)
{
	if (animation->finished)
		return;
	animation->finished = true;
	if (animation->impl->finish) {
		animation->impl->finish(animation);
	}
	wl_signal_emit_mutable(&animation->events.finished, NULL);
	animation_destroy(animation);
}

static void
animation_run(struct animation *animation, double t)
{
	animation->impl->run(animation, t);
}

static double
animation_elapsed_time(const struct animation *animation)
{
	int64_t now = gettime_us();
	int64_t elapsed = now - animation->start_time;
	return fmin(elapsed * 1e-6 / animation->duration, 1.0);
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

	double t = animation_elapsed_time(animation);

	animation_run(animation, t);

	if (t < 1.0) {
		wlr_output_schedule_frame(event->output);
	} else {
		animation_finish(animation);
	}
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
animation_init(struct animation *animation, struct animation_impl *impl,
		double duration, struct view *view)
{
	if (!view->output || !view->output->wlr_output) {
		return;
	}

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

	animation->impl = impl;
	animation->duration = duration;
	wl_signal_init(&animation->events.finished);

	animation->start_time = gettime_us();
	animation_run(animation, 0);
	wlr_output_schedule_frame(animation->wlr_output);
}
