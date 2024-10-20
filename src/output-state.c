// SPDX-License-Identifier: GPL-2.0-only

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_management_v1.h>
#include "labwc.h"
#include "output-state.h"

void
output_state_init(struct output *output)
{
	wlr_output_state_init(&output->pending);

	/*
	 * As there is no direct way to convert an existing output
	 * configuration to an output_state we first convert it
	 * to a temporary output-management config and then apply
	 * it to an empty wlr_output_state.
	 */
	struct wlr_output_configuration_v1 *backup_config =
		wlr_output_configuration_v1_create();
	struct wlr_output_configuration_head_v1 *backup_head =
		wlr_output_configuration_head_v1_create(
			backup_config, output->wlr_output);

	wlr_output_head_v1_state_apply(&backup_head->state, &output->pending);
	wlr_output_configuration_v1_destroy(backup_config);
}

bool
wlr_output_test(struct wlr_output *wlr_output)
{
	struct output *output = wlr_output->data;
	return wlr_output_test_state(wlr_output, &output->pending);
}

bool
wlr_output_commit(struct wlr_output *wlr_output)
{
	struct output *output = wlr_output->data;
	bool committed = wlr_output_commit_state(wlr_output, &output->pending);
	if (committed) {
		wlr_output_state_finish(&output->pending);
		wlr_output_state_init(&output->pending);
	} else {
		wlr_log(WLR_ERROR, "Failed to commit frame");
	}
	return committed;
}

void
wlr_output_enable(struct wlr_output *wlr_output, bool enabled)
{
	struct output *output = wlr_output->data;
	wlr_output_state_set_enabled(&output->pending, enabled);
}

void
wlr_output_set_mode(struct wlr_output *wlr_output, struct wlr_output_mode *mode)
{
	struct output *output = wlr_output->data;
	wlr_output_state_set_mode(&output->pending, mode);
}

void
wlr_output_set_custom_mode(struct wlr_output *wlr_output,
		int32_t width, int32_t height, int32_t refresh)
{
	struct output *output = wlr_output->data;
	wlr_output_state_set_custom_mode(&output->pending, width, height, refresh);
}

void
wlr_output_set_scale(struct wlr_output *wlr_output, float scale)
{
	struct output *output = wlr_output->data;
	wlr_output_state_set_scale(&output->pending, scale);
}

void
wlr_output_set_transform(struct wlr_output *wlr_output,
		enum wl_output_transform transform)
{
	struct output *output = wlr_output->data;
	wlr_output_state_set_transform(&output->pending, transform);
}

void
wlr_output_enable_adaptive_sync(struct wlr_output *wlr_output, bool enabled)
{
	struct output *output = wlr_output->data;
	wlr_output_state_set_adaptive_sync_enabled(&output->pending, enabled);
}
