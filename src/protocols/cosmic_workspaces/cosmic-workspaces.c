// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "common/array.h"
#include "common/mem.h"
#include "common/list.h"
#include "cosmic-workspace-unstable-v1-protocol.h"
#include "protocols/cosmic-workspaces.h"
#include "protocols/cosmic-workspaces-internal.h"

/*
 *	.--------------------.
 *	|        TODO        |
 *	|--------------------|
 *	| - prevent empty    |
 *	|   done events      |
 *	| - go through xml   |
 *	|   and verify impl  |
 *	| - assert pub API   |
 *	`--------------------´
 *
 */

/* Only used within an assert() */
#ifndef NDEBUG
	#define COSMIC_WORKSPACE_V1_VERSION 1
#endif

/* These are just *waaay* too long */
#define ZCOSMIC_CAP_WS_CREATE \
	ZCOSMIC_WORKSPACE_GROUP_HANDLE_V1_ZCOSMIC_WORKSPACE_GROUP_CAPABILITIES_V1_CREATE_WORKSPACE
#define ZCOSMIC_CAP_WS_ACTIVATE \
	ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_ACTIVATE
#define ZCOSMIC_CAP_WS_DEACTIVATE \
	ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_DEACTIVATE
#define ZCOSMIC_CAP_WS_REMOVE \
	ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_REMOVE

enum workspace_state {
	CW_WS_STATE_ACTIVE  = 1 << 0,
	CW_WS_STATE_URGENT  = 1 << 1,
	CW_WS_STATE_HIDDEN  = 1 << 2,

	/*
	 * Set when creating a new workspace so we
	 * don't end up having to send the state twice.
	 */
	CW_WS_STATE_INVALID = 1 << 31,
};

static void
add_caps(struct wl_array *caps_arr, uint32_t caps)
{
	if (caps == CW_CAP_NONE) {
		return;
	}
	if (caps & CW_CAP_GRP_WS_CREATE) {
		array_add(caps_arr, ZCOSMIC_CAP_WS_CREATE);
	}
	if (caps & CW_CAP_WS_ACTIVATE) {
		array_add(caps_arr, ZCOSMIC_CAP_WS_ACTIVATE);
	}
	if (caps & CW_CAP_WS_DEACTIVATE) {
		array_add(caps_arr, ZCOSMIC_CAP_WS_DEACTIVATE);
	}
	if (caps & CW_CAP_WS_REMOVE) {
		array_add(caps_arr, ZCOSMIC_CAP_WS_REMOVE);
	}
}

/* Workspace */
static void
workspace_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
workspace_handle_activate(struct wl_client *client, struct wl_resource *resource)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		/* workspace was destroyed from the compositor side */
		return;
	}
	struct lab_cosmic_workspace *workspace = addon->data;
	transaction_add_workspace_ev(workspace, resource, CW_PENDING_WS_ACTIVATE);
}

static void
workspace_handle_deactivate(struct wl_client *client, struct wl_resource *resource)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		/* Workspace was destroyed from the compositor side */
		return;
	}
	struct lab_cosmic_workspace *workspace = addon->data;
	transaction_add_workspace_ev(workspace, resource, CW_PENDING_WS_DEACTIVATE);
}

static void
workspace_handle_remove(struct wl_client *client, struct wl_resource *resource)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		/* workspace was destroyed from the compositor side */
		return;
	}
	struct lab_cosmic_workspace *workspace = addon->data;
	transaction_add_workspace_ev(workspace, resource, CW_PENDING_WS_REMOVE);
}

static const struct zcosmic_workspace_handle_v1_interface workspace_impl = {
	.destroy = workspace_handle_destroy,
	.activate = workspace_handle_activate,
	.deactivate = workspace_handle_deactivate,
	.remove = workspace_handle_remove,
};

static void
workspace_instance_resource_destroy(struct wl_resource *resource)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (addon) {
		resource_addon_destroy(addon);
		wl_resource_set_user_data(resource, NULL);
	}

	wl_list_remove(wl_resource_get_link(resource));
}

static struct wl_resource *
workspace_resource_create(struct lab_cosmic_workspace *workspace,
		struct wl_resource *group_resource, struct session_context *ctx)
{
	struct wl_client *client = wl_resource_get_client(group_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&zcosmic_workspace_handle_v1_interface,
			wl_resource_get_version(group_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	struct wl_resource_addon *addon = resource_addon_create(ctx);
	addon->data = workspace;

	wl_resource_set_implementation(resource, &workspace_impl, addon,
		workspace_instance_resource_destroy);

	wl_list_insert(&workspace->resources, wl_resource_get_link(resource));
	return resource;
}

/* Workspace internal helpers */
static void
workspace_send_state(struct lab_cosmic_workspace *workspace, struct wl_resource *target)
{
	struct wl_array state;
	wl_array_init(&state);

	if (workspace->state & CW_WS_STATE_ACTIVE) {
		array_add(&state, ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_ACTIVE);
	}
	if (workspace->state & CW_WS_STATE_URGENT) {
		array_add(&state, ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_URGENT);
	}
	if (workspace->state & CW_WS_STATE_HIDDEN) {
		array_add(&state, ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_HIDDEN);
	}

	if (target) {
		zcosmic_workspace_handle_v1_send_state(target, &state);
	} else {
		struct wl_resource *resource;
		wl_resource_for_each(resource, &workspace->resources) {
			zcosmic_workspace_handle_v1_send_state(resource, &state);
		}
	}

	wl_array_release(&state);
}

static void
workspace_send_initial_state(struct lab_cosmic_workspace *workspace, struct wl_resource *resource)
{
	zcosmic_workspace_handle_v1_send_capabilities(resource, &workspace->capabilities);
	if (workspace->coordinates.size > 0) {
		zcosmic_workspace_handle_v1_send_coordinates(resource, &workspace->coordinates);
	}
	if (workspace->name) {
		zcosmic_workspace_handle_v1_send_name(resource, workspace->name);
	}
}

static void
workspace_set_state(struct lab_cosmic_workspace *workspace,
		enum workspace_state state, bool enabled)
{
	if (!!(workspace->state_pending & state) == enabled) {
		return;
	}

	if (enabled) {
		workspace->state_pending |= state;
	} else {
		workspace->state_pending &= ~state;
	}
	manager_schedule_done_event(workspace->group->manager);
}

/* Group */
static void
group_handle_create_workspace(struct wl_client *client,
		struct wl_resource *resource, const char *name)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		return;
	}

	struct lab_cosmic_workspace_group *group = addon->data;
	transaction_add_workspace_group_ev(group, resource, CW_PENDING_WS_CREATE, name);
}

static void
group_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct zcosmic_workspace_group_handle_v1_interface group_impl = {
	.create_workspace = group_handle_create_workspace,
	.destroy = group_handle_destroy,
};

static void
group_instance_resource_destroy(struct wl_resource *resource)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (addon) {
		resource_addon_destroy(addon);
		wl_resource_set_user_data(resource, NULL);
	}
	wl_list_remove(wl_resource_get_link(resource));
}

static struct wl_resource *
group_resource_create(struct lab_cosmic_workspace_group *group,
		struct wl_resource *manager_resource, struct session_context *ctx)
{
	struct wl_client *client = wl_resource_get_client(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&zcosmic_workspace_group_handle_v1_interface,
			wl_resource_get_version(manager_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	struct wl_resource_addon *addon = resource_addon_create(ctx);
	addon->data = group;

	wl_resource_set_implementation(resource, &group_impl, addon,
		group_instance_resource_destroy);

	wl_list_insert(&group->resources, wl_resource_get_link(resource));
	return resource;
}

/* Group internal helpers */
static void
group_send_state(struct lab_cosmic_workspace_group *group, struct wl_resource *resource)
{
	zcosmic_workspace_group_handle_v1_send_capabilities(
		resource, &group->capabilities);

	group_output_send_initial_state(group, resource);
}

/* Manager itself */
static void
manager_handle_commit(struct wl_client *client, struct wl_resource *resource)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		return;
	}

	struct transaction_group *trans_grp;
	struct transaction_workspace *trans_ws;
	struct transaction *trans, *trans_tmp;
	wl_list_for_each_safe(trans, trans_tmp, &addon->ctx->transactions, link) {
		switch (trans->change) {
		case CW_PENDING_WS_CREATE:
			trans_grp = wl_container_of(trans, trans_grp, base);
			wl_signal_emit_mutable(
				&trans_grp->group->events.create_workspace,
				trans_grp->new_workspace_name);
			free(trans_grp->new_workspace_name);
			break;
		case CW_PENDING_WS_ACTIVATE:
			trans_ws = wl_container_of(trans, trans_ws, base);
			wl_signal_emit_mutable(&trans_ws->workspace->events.activate, NULL);
			break;
		case CW_PENDING_WS_DEACTIVATE:
			trans_ws = wl_container_of(trans, trans_ws, base);
			wl_signal_emit_mutable(&trans_ws->workspace->events.deactivate, NULL);
			break;
		case CW_PENDING_WS_REMOVE:
			trans_ws = wl_container_of(trans, trans_ws, base);
			wl_signal_emit_mutable(&trans_ws->workspace->events.remove, NULL);
			break;
		default:
			wlr_log(WLR_ERROR, "Invalid transaction state: %u", trans->change);
		}
		wl_list_remove(&trans->link);
		free(trans);
	}
}

static void
manager_handle_stop(struct wl_client *client, struct wl_resource *resource)
{
	zcosmic_workspace_manager_v1_send_finished(resource);
	wl_resource_destroy(resource);
}

static const struct zcosmic_workspace_manager_v1_interface manager_impl = {
	.commit = manager_handle_commit,
	.stop = manager_handle_stop,
};

static void
manager_instance_resource_destroy(struct wl_resource *resource)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (addon) {
		resource_addon_destroy(addon);
		wl_resource_set_user_data(resource, NULL);
	}

	wl_list_remove(wl_resource_get_link(resource));
}

static void
manager_handle_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id)
{
	struct lab_cosmic_workspace_manager *manager = data;
	struct wl_resource *resource = wl_resource_create(client,
			&zcosmic_workspace_manager_v1_interface,
			version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	struct wl_resource_addon *addon = resource_addon_create(/* session context*/ NULL);
	addon->data = manager;

	wl_resource_set_implementation(resource, &manager_impl,
		addon, manager_instance_resource_destroy);

	wl_list_insert(&manager->resources, wl_resource_get_link(resource));

	struct lab_cosmic_workspace *workspace;
	struct lab_cosmic_workspace_group *group;
	wl_list_for_each(group, &manager->groups, link) {
		/* Create group resource */
		struct wl_resource *group_resource =
			group_resource_create(group, resource, addon->ctx);
		zcosmic_workspace_manager_v1_send_workspace_group(resource, group_resource);
		group_send_state(group, group_resource);

		/* Create workspace resource */
		wl_list_for_each(workspace, &group->workspaces, link) {
			struct wl_resource *workspace_resource =
				workspace_resource_create(workspace, group_resource, addon->ctx);
			zcosmic_workspace_group_handle_v1_send_workspace(
				group_resource, workspace_resource);
			workspace_send_initial_state(workspace, workspace_resource);
			/* Send the current workspace state manually */
			workspace_send_state(workspace, workspace_resource);
		}
	}
	zcosmic_workspace_manager_v1_send_done(resource);
}

static void
manager_handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct lab_cosmic_workspace_manager *manager =
		wl_container_of(listener, manager, on.display_destroy);

	struct lab_cosmic_workspace_group *group, *tmp;
	wl_list_for_each_safe(group, tmp, &manager->groups, link) {
		lab_cosmic_workspace_group_destroy(group);
	}

	if (manager->idle_source) {
		wl_event_source_remove(manager->idle_source);
	}

	wl_list_remove(&manager->on.display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

/* Manager internal helpers */
static void
manager_idle_send_done(void *data)
{
	struct lab_cosmic_workspace_manager *manager = data;

	struct lab_cosmic_workspace *workspace;
	struct lab_cosmic_workspace_group *group;
	wl_list_for_each(group, &manager->groups, link) {
		wl_list_for_each(workspace, &group->workspaces, link) {
			if (workspace->state != workspace->state_pending) {
				workspace->state = workspace->state_pending;
				workspace_send_state(workspace, /*target*/ NULL);
			}
		}
	}

	struct wl_resource *resource;
	wl_resource_for_each(resource, &manager->resources) {
		zcosmic_workspace_manager_v1_send_done(resource);
	}
	manager->idle_source = NULL;
}

/* Internal API */
void
manager_schedule_done_event(struct lab_cosmic_workspace_manager *manager)
{
	if (manager->idle_source) {
		return;
	}
	if (!manager->event_loop) {
		return;
	}
	manager->idle_source = wl_event_loop_add_idle(
		manager->event_loop, manager_idle_send_done, manager);
}

/* Public API */
struct lab_cosmic_workspace_manager *
lab_cosmic_workspace_manager_create(struct wl_display *display, uint32_t caps, uint32_t version)
{
	assert(version <= COSMIC_WORKSPACE_V1_VERSION);

	struct lab_cosmic_workspace_manager *manager = znew(*manager);
	manager->global = wl_global_create(display,
			&zcosmic_workspace_manager_v1_interface,
			version, manager, manager_handle_bind);

	if (!manager->global) {
		free(manager);
		return NULL;
	}

	manager->caps = caps;
	manager->event_loop = wl_display_get_event_loop(display);

	manager->on.display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->on.display_destroy);

	wl_list_init(&manager->groups);
	wl_list_init(&manager->resources);
	return manager;
}

struct lab_cosmic_workspace_group *
lab_cosmic_workspace_group_create(struct lab_cosmic_workspace_manager *manager)
{
	assert(manager);

	struct lab_cosmic_workspace_group *group = znew(*group);
	group->manager = manager;

	wl_array_init(&group->capabilities);
	add_caps(&group->capabilities, manager->caps & CW_CAP_GRP_ALL);

	wl_list_init(&group->outputs);
	wl_list_init(&group->resources);
	wl_list_init(&group->workspaces);
	wl_signal_init(&group->events.create_workspace);
	wl_signal_init(&group->events.destroy);

	wl_list_append(&manager->groups, &group->link);

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &manager->resources) {
		struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
		assert(addon && addon->ctx);
		struct wl_resource *group_resource =
			group_resource_create(group, resource, addon->ctx);
		zcosmic_workspace_manager_v1_send_workspace_group(resource, group_resource);
		group_send_state(group, group_resource);
	}
	manager_schedule_done_event(manager);

	return group;
}

void
lab_cosmic_workspace_group_destroy(struct lab_cosmic_workspace_group *group)
{
	if (!group) {
		return;
	}
	wl_signal_emit_mutable(&group->events.destroy, NULL);

	struct lab_cosmic_workspace *ws, *ws_tmp;
	wl_list_for_each_safe(ws, ws_tmp, &group->workspaces, link) {
		lab_cosmic_workspace_destroy(ws);
	}

	struct wl_resource *resource, *res_tmp;
	wl_resource_for_each_safe(resource, res_tmp, &group->resources) {
		struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
		if (addon) {
			resource_addon_destroy(addon);
			wl_resource_set_user_data(resource, NULL);
		}
		zcosmic_workspace_group_handle_v1_send_remove(resource);
		wl_list_remove(wl_resource_get_link(resource));
		wl_list_init(wl_resource_get_link(resource));
	}

	wl_list_remove(&group->link);
	wl_array_release(&group->capabilities);
	free(group);
}

struct lab_cosmic_workspace *
lab_cosmic_workspace_create(struct lab_cosmic_workspace_group *group)
{
	assert(group);

	struct lab_cosmic_workspace *workspace = znew(*workspace);
	workspace->group = group;
	/*
	 * Ensures we are sending workspace->state_pending on the done event,
	 * regardless if the compositor has changed any state in between here
	 * and the scheduled done event or not.
	 *
	 * Without this we might have to send the state twice, first here and
	 * then again in the scheduled done event when there were any changes.
	 */
	workspace->state = CW_WS_STATE_INVALID;

	wl_array_init(&workspace->capabilities);
	add_caps(&workspace->capabilities, group->manager->caps & CW_CAP_WS_ALL);

	wl_list_init(&workspace->resources);
	wl_array_init(&workspace->coordinates);
	wl_signal_init(&workspace->events.activate);
	wl_signal_init(&workspace->events.deactivate);
	wl_signal_init(&workspace->events.remove);
	wl_signal_init(&workspace->events.destroy);

	wl_list_append(&group->workspaces, &workspace->link);

	/* Notify clients */
	struct wl_resource *group_resource;
	wl_resource_for_each(group_resource, &group->resources) {
		struct wl_resource_addon *addon = wl_resource_get_user_data(group_resource);
		assert(addon && addon->ctx);
		struct wl_resource *workspace_resource =
			workspace_resource_create(workspace, group_resource, addon->ctx);
		zcosmic_workspace_group_handle_v1_send_workspace(
			group_resource, workspace_resource);
		workspace_send_initial_state(workspace, workspace_resource);
	}
	manager_schedule_done_event(group->manager);

	return workspace;
}

void
lab_cosmic_workspace_set_name(struct lab_cosmic_workspace *workspace, const char *name)
{
	assert(workspace);
	assert(name);

	if (!workspace->name || strcmp(workspace->name, name)) {
		free(workspace->name);
		workspace->name = xstrdup(name);
		struct wl_resource *resource;
		wl_resource_for_each(resource, &workspace->resources) {
			zcosmic_workspace_handle_v1_send_name(resource, workspace->name);
		}
	}
	manager_schedule_done_event(workspace->group->manager);
}

void
lab_cosmic_workspace_set_active(struct lab_cosmic_workspace *workspace, bool enabled)
{
	workspace_set_state(workspace, CW_WS_STATE_ACTIVE, enabled);
}

void
lab_cosmic_workspace_set_urgent(struct lab_cosmic_workspace *workspace, bool enabled)
{
	workspace_set_state(workspace, CW_WS_STATE_URGENT, enabled);
}

void
lab_cosmic_workspace_set_hidden(struct lab_cosmic_workspace *workspace, bool enabled)
{
	workspace_set_state(workspace, CW_WS_STATE_HIDDEN, enabled);
}

void
lab_cosmic_workspace_set_coordinates(struct lab_cosmic_workspace *workspace,
		struct wl_array *coordinates)
{
	wl_array_release(&workspace->coordinates);
	wl_array_init(&workspace->coordinates);
	wl_array_copy(&workspace->coordinates, coordinates);

	struct wl_resource *resource;
	wl_resource_for_each(resource, &workspace->resources) {
		zcosmic_workspace_handle_v1_send_coordinates(resource, &workspace->coordinates);
	}
	manager_schedule_done_event(workspace->group->manager);
}

void
lab_cosmic_workspace_destroy(struct lab_cosmic_workspace *workspace)
{
	if (!workspace) {
		return;
	}
	wl_signal_emit_mutable(&workspace->events.destroy, NULL);

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &workspace->resources) {
		struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
		if (addon) {
			resource_addon_destroy(addon);
			wl_resource_set_user_data(resource, NULL);
		}
		zcosmic_workspace_handle_v1_send_remove(resource);
		wl_list_remove(wl_resource_get_link(resource));
		wl_list_init(wl_resource_get_link(resource));
	}
	manager_schedule_done_event(workspace->group->manager);

	wl_list_remove(&workspace->link);
	wl_array_release(&workspace->coordinates);
	wl_array_release(&workspace->capabilities);
	zfree(workspace->name);
	free(workspace);
}
