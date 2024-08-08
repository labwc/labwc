/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_COSMIC_WORKSPACES_H
#define LABWC_PROTOCOLS_COSMIC_WORKSPACES_H

#include <stdbool.h>
#include <wayland-server-core.h>

struct wlr_output;

struct lab_cosmic_workspace_manager {
	struct wl_global *global;
	struct wl_list groups;
	uint32_t caps;
	struct wl_event_source *idle_source;
	struct wl_event_loop *event_loop;

	struct {
		struct wl_listener display_destroy;
	} on;

	struct wl_list resources;
};

struct lab_cosmic_workspace_group {
	struct lab_cosmic_workspace_manager *manager;
	struct wl_list workspaces;
	struct wl_array capabilities;
	struct {
		struct wl_signal create_workspace;
		struct wl_signal destroy;
	} events;

	struct wl_list link;
	struct wl_list outputs;
	struct wl_list resources;
};

struct lab_cosmic_workspace {
	struct lab_cosmic_workspace_group *group;
	char *name;
	struct wl_array coordinates;
	struct wl_array capabilities;
	uint32_t state;         /* enum lab_cosmic_workspace_state */
	uint32_t state_pending; /* enum lab_cosmic_workspace_state */

	struct {
		struct wl_signal activate;
		struct wl_signal deactivate;
		struct wl_signal remove;
		struct wl_signal destroy;
	} events;

	struct wl_list link;
	struct wl_list resources;
};

enum lab_cosmic_workspace_caps {
	CW_CAP_NONE          = 0,
	CW_CAP_GRP_ALL       = 0x000000ff,
	CW_CAP_WS_ALL        = 0x0000ff00,

	/* group caps */
	CW_CAP_GRP_WS_CREATE = 1 << 0,

	/* workspace caps */
	CW_CAP_WS_ACTIVATE   = 1 << 8,
	CW_CAP_WS_DEACTIVATE = 1 << 9,
	CW_CAP_WS_REMOVE     = 1 << 10,
};

struct lab_cosmic_workspace_manager *lab_cosmic_workspace_manager_create(
	struct wl_display *display, uint32_t caps, uint32_t version);

struct lab_cosmic_workspace_group *lab_cosmic_workspace_group_create(
	struct lab_cosmic_workspace_manager *manager);

void lab_cosmic_workspace_group_output_enter(
	struct lab_cosmic_workspace_group *group, struct wlr_output *output);

void lab_cosmic_workspace_group_output_leave(

	struct lab_cosmic_workspace_group *group, struct wlr_output *output);
void lab_cosmic_workspace_group_destroy(struct lab_cosmic_workspace_group *group);

struct lab_cosmic_workspace *lab_cosmic_workspace_create(struct lab_cosmic_workspace_group *group);
void lab_cosmic_workspace_set_name(struct lab_cosmic_workspace *workspace, const char *name);
void lab_cosmic_workspace_set_active(struct lab_cosmic_workspace *workspace, bool enabled);
void lab_cosmic_workspace_set_urgent(struct lab_cosmic_workspace *workspace, bool enabled);
void lab_cosmic_workspace_set_hidden(struct lab_cosmic_workspace *workspace, bool enabled);
void lab_cosmic_workspace_set_coordinates(struct lab_cosmic_workspace *workspace,
	struct wl_array *coordinates);
void lab_cosmic_workspace_destroy(struct lab_cosmic_workspace *workspace);

#endif /* LABWC_PROTOCOLS_COSMIC_WORKSPACES_H */
