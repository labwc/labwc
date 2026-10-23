#include "animation.h"
#include "common/mem.h"
#include <wlr/types/wlr_scene.h>

static struct scale_animation *
scale_animation_from_animation(struct animation *animation)
{
	return (struct scale_animation*)animation;
}

static void
set_scale(struct scale_animation *scale_animation, double h_scale, double v_scale)
{
	struct animation *animation = &scale_animation->base;

	/* The box starts out empty when there's no buffer set */
	int width = 1, height = 1;
	struct wlr_box src;
	wlr_scene_node_get_source_box(&animation->transform->node, &src);
	if (!wlr_box_empty(&src)) {
		/* Empty dst_box disables the transform */
		width = fmax(round(src.width * h_scale), 1);
		height = fmax(round(src.height * v_scale), 1);
	}

	struct wlr_box dst = {
		.x = src.x + (src.width - width) / 2,
		.y = src.y + (src.height - height) / 2,
		.width = width,
		.height = height,
	};
	wlr_scene_transform_set_dst_box(animation->transform, &dst);
}

static void
scale_animation_run(struct animation *animation, double t)
{
	struct scale_animation *scale_animation =
		scale_animation_from_animation(animation);
	double h_scale = fmax(0.0, scale_animation->h_curve(t));
	double v_scale = fmax(0.0, scale_animation->v_curve(t));
	set_scale(scale_animation, h_scale, v_scale);
}

static void
scale_animation_finish(struct animation *animation)
{
	struct wlr_box empty = { 0 };
	wlr_scene_transform_set_dst_box(animation->transform, &empty);
}

static struct animation_impl scale_animation_impl = {
	.run = scale_animation_run,
	.finish = scale_animation_finish,
};

struct animation *
scale_animation_start(struct view *view, double duration,
		double (*h_curve)(double t), double (*v_curve)(double t))
{
	struct scale_animation *scale_animation = znew(*scale_animation);
	scale_animation->h_curve = h_curve;
	scale_animation->v_curve = v_curve;
	animation_init(&scale_animation->base, &scale_animation_impl, duration, view);
	return &scale_animation->base;
}
