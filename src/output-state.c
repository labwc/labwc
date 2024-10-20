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
output_state_commit(struct output *output)
{
	bool committed =
		wlr_output_commit_state(output->wlr_output, &output->pending);
	if (committed) {
		wlr_output_state_finish(&output->pending);
		wlr_output_state_init(&output->pending);
	} else {
		wlr_log(WLR_ERROR, "Failed to commit frame");
	}
	return committed;
}
