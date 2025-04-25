// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include "common/scene-helpers.h"
#include "labwc.h"
#include "magnifier.h"
#include "output-state.h"

struct wlr_surface *
lab_wlr_surface_from_node(struct wlr_scene_node *node)
{
	struct wlr_scene_buffer *buffer;
	struct wlr_scene_surface *scene_surface;

	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		buffer = wlr_scene_buffer_from_node(node);
		scene_surface = wlr_scene_surface_try_from_buffer(buffer);
		if (scene_surface) {
			return scene_surface->surface;
		}
	}
	return NULL;
}

struct wlr_scene_node *
lab_wlr_scene_get_prev_node(struct wlr_scene_node *node)
{
	assert(node);
	struct wlr_scene_node *prev;
	prev = wl_container_of(node->link.prev, node, link);
	if (&prev->link == &node->parent->children) {
		return NULL;
	}
	return prev;
}

/*
 * This is a slightly modified copy of scene_output_damage(),
 * required to properly add the magnifier damage to scene_output
 * ->damage_ring and scene_output->pending_commit_damage.
 *
 * The only difference is code style and removal of wlr_output_schedule_frame().
 */
static void
scene_output_damage(struct wlr_scene_output *scene_output,
		const pixman_region32_t *damage)
{
	struct wlr_output *output = scene_output->output;

	pixman_region32_t clipped;
	pixman_region32_init(&clipped);
	pixman_region32_intersect_rect(&clipped, damage, 0, 0, output->width, output->height);

	if (pixman_region32_not_empty(&clipped)) {
		wlr_damage_ring_add(&scene_output->damage_ring, &clipped);
		pixman_region32_union(&scene_output->pending_commit_damage,
			&scene_output->pending_commit_damage, &clipped);
	}

	pixman_region32_fini(&clipped);
}

/*
 * This is a copy of wlr_scene_output_commit()
 * as it doesn't use the pending state at all.
 */
bool
lab_wlr_scene_output_commit(struct wlr_scene_output *scene_output,
		struct wlr_output_state *state)
{
	assert(scene_output);
	assert(state);
	struct wlr_output *wlr_output = scene_output->output;
	struct output *output = wlr_output->data;
	bool wants_magnification = output_wants_magnification(output);

	/*
	 * FIXME: Regardless of wants_magnification, we are currently adding
	 * damages to next frame when magnifier is shown, which forces
	 * rendering on every output commit and overloads CPU.
	 * We also need to verify the necessity of wants_magnification.
	 */
	if (!wlr_output->needs_frame && !pixman_region32_not_empty(
			&scene_output->pending_commit_damage) && !wants_magnification) {
		return true;
	}

	if (!wlr_scene_output_build_state(scene_output, state, NULL)) {
		wlr_log(WLR_ERROR, "Failed to build output state for %s",
			wlr_output->name);
		return false;
	}

	if (state->tearing_page_flip) {
		if (!wlr_output_test_state(wlr_output, state)) {
			state->tearing_page_flip = false;
		}
	}

	struct wlr_box additional_damage = {0};
	if (state->buffer && magnifier_is_enabled()) {
		magnifier_draw(output, state->buffer, &additional_damage);
	}

	bool committed = wlr_output_commit_state(wlr_output, state);
	/*
	 * Handle case where the output state test for tearing succeeded,
	 * but actual commit failed. Retry without tearing.
	 */
	if (!committed && state->tearing_page_flip) {
		state->tearing_page_flip = false;
		committed = wlr_output_commit_state(wlr_output, state);
	}
	if (committed) {
		if (state == &output->pending) {
			wlr_output_state_finish(&output->pending);
			wlr_output_state_init(&output->pending);
		}
	} else {
		wlr_log(WLR_INFO, "Failed to commit output %s",
			wlr_output->name);
		return false;
	}

	if (!wlr_box_empty(&additional_damage)) {
		pixman_region32_t region;
		pixman_region32_init_rect(&region,
			additional_damage.x, additional_damage.y,
			additional_damage.width, additional_damage.height);
		scene_output_damage(scene_output, &region);
		pixman_region32_fini(&region);
	}

	return true;
}
