#include "animation.h"
#include "common/mem.h"
#include <wlr/types/wlr_scene.h>

struct animation *
zoom_animation_start(struct view *view, double duration,
		double (*curve)(double t))
{
	return scale_animation_start(view, duration, curve, curve);
}
