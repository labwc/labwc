/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_COSMIC_WORKSPACES_INTERNAL_H
#define LABWC_PROTOCOLS_COSMIC_WORKSPACES_INTERNAL_H

struct lab_cosmic_workspace;
struct lab_cosmic_workspace_group;
struct lab_cosmic_workspace_manager;

enum pending_change {
	/* group events */
	CW_PENDING_WS_CREATE     = 1 << 0,

	/* ws events*/
	CW_PENDING_WS_ACTIVATE   = 1 << 1,
	CW_PENDING_WS_DEACTIVATE = 1 << 2,
	CW_PENDING_WS_REMOVE     = 1 << 3,
};

void cosmic_group_output_send_initial_state(struct lab_cosmic_workspace_group *group,
	struct wl_resource *group_resource);

void cosmic_manager_schedule_done_event(struct lab_cosmic_workspace_manager *manager);

#endif /* LABWC_PROTOCOLS_COSMIC_WORKSPACES_INTERNAL_H */
