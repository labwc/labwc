/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_EXT_WORKSPACES_INTERNAL_H
#define LABWC_PROTOCOLS_EXT_WORKSPACES_INTERNAL_H

struct wl_resource;
struct lab_ext_workspace_group;
struct lab_ext_workspace_manager;

enum pending_ext_workspaces_change {
	/* group events */
	WS_PENDING_WS_CREATE     = 1 << 0,

	/* ws events*/
	WS_PENDING_WS_ACTIVATE   = 1 << 1,
	WS_PENDING_WS_DEACTIVATE = 1 << 2,
	WS_PENDING_WS_REMOVE     = 1 << 3,
	WS_PENDING_WS_ASSIGN     = 1 << 4,
};

void ext_group_output_send_initial_state(struct lab_ext_workspace_group *group,
	struct wl_resource *group_resource);

void ext_manager_schedule_done_event(struct lab_ext_workspace_manager *manager);

#endif /* LABWC_PROTOCOLS_EXT_WORKSPACES_INTERNAL_H */
