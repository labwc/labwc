/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ANIMATION_H
#define LABWC_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-server-core.h>

struct view;

struct animation {
	struct animation_impl *impl;
	int64_t start_time;
	double duration;
	bool finished;

	struct wlr_scene_transform *transform;
	struct wlr_output *wlr_output;

	struct wl_listener output_commit;
	struct wl_listener output_destroy;
	struct wl_listener view_destroy;

	struct {
		struct wl_signal finished;
	} events;
};

struct scale_animation {
	struct animation base;
	double (*h_curve)(double t);
	double (*v_curve)(double t);
};

struct animation_impl {
	void (*run)(struct animation *self, double t);
	void (*finish)(struct animation *self);
};

void animation_init(struct animation *animation, struct animation_impl *impl,
		double duration, struct view *view);
void animation_finish(struct animation *animation);

struct animation *scale_animation_start(struct view *view, double duration,
		double (*h_curve)(double t), double (*v_curve)(double t));
struct animation *zoom_animation_start(struct view *view, double duration,
		double (*curve)(double t));

double animation_curve_linear_in(double t);
double animation_curve_linear_out(double t);
double animation_curve_elastic_in(double t);
double animation_curve_elastic_out(double t);
double animation_curve_bounce_in(double t);
double animation_curve_bounce_out(double t);
double animation_curve_overshoot_in(double t);
double animation_curve_overshoot_out(double t);

#endif /* LABWC_ANIMATION_H */
