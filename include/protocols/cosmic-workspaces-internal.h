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

struct transaction {
	uint32_t change;
	struct wl_list link;
};

struct transaction_workspace {
	struct transaction base;
	struct lab_cosmic_workspace *workspace;
};

struct transaction_group {
	struct transaction base;
	struct lab_cosmic_workspace_group *group;
	char *new_workspace_name;
};

struct session_context {
	int ref_count;
	struct wl_list transactions;
};

struct wl_resource_addon {
	struct session_context *ctx;
	void *data;
};

struct wl_resource_addon *resource_addon_create(struct session_context *ctx);

void transaction_add_workspace_ev(struct lab_cosmic_workspace *ws,
	struct wl_resource *resource, enum pending_change change);

void transaction_add_workspace_group_ev(struct lab_cosmic_workspace_group *group,
	struct wl_resource *resource, enum pending_change change,
	const char *new_workspace_name);

void resource_addon_destroy(struct wl_resource_addon *addon);

void group_output_send_initial_state(struct lab_cosmic_workspace_group *group,
	struct wl_resource *group_resource);

void manager_schedule_done_event(struct lab_cosmic_workspace_manager *manager);

#endif /* LABWC_PROTOCOLS_COSMIC_WORKSPACES_INTERNAL_H */
