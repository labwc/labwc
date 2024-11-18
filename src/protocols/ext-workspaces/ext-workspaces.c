// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "common/array.h"
#include "common/mem.h"
#include "common/list.h"
#include "ext-workspace-v1-protocol.h"
#include "protocols/ext-workspaces.h"
#include "protocols/ext-workspaces-internal.h"
#include "protocols/transaction-addon.h"

/*
 *	.--------------------.
 *	|        TODO        |
 *	|--------------------|
 *	| - go through xml   |
 *	|   and verify impl  |
 *	| - assert pub API   |
 *	`--------------------´
 *
 */

/*
 * TODO:
 *
 * - Add something like addon_mark_dirty(wl_resource, bool) that sets a
 *   bool in the addon context whenever we did something that requires
 *   a done event by the manager. Then in the manager done idle event
 *   check against that bool and only send the done event if the context
 *   is dirty. After that set the context to non-dirty. This should
 *   ensure that clients do not receive empty done events for things
 *   like output_enter / output_leave without having the output bound
 *   on the client side.
 */

/* Only used within an assert() */
#ifndef NDEBUG
	#define EXT_WORKSPACE_V1_VERSION 1
#endif

/*
 * Set when creating a new workspace state so we
 * don't end up having to send the state twice.
 */
#define WS_STATE_INVALID 0xffffffff

/* Workspace */
static void
workspace_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
workspace_handle_activate(struct wl_client *client, struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		/* workspace was destroyed from the compositor side */
		return;
	}
	struct lab_ext_workspace *workspace = addon->data;
	lab_transaction_add(addon->ctx, WS_PENDING_WS_ACTIVATE, workspace, /*argument*/ NULL);
}

static void
workspace_handle_deactivate(struct wl_client *client, struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		/* Workspace was destroyed from the compositor side */
		return;
	}
	struct lab_ext_workspace *workspace = addon->data;
	lab_transaction_add(addon->ctx, WS_PENDING_WS_DEACTIVATE, workspace, /*argument*/ NULL);
}

static void
workspace_handle_assign(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *new_group_resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		/* workspace was destroyed from the compositor side */
		return;
	}
	struct lab_ext_workspace *workspace = addon->data;
	struct lab_wl_resource_addon *grp_addon = wl_resource_get_user_data(new_group_resource);
	if (!grp_addon) {
		/* group was destroyed from the compositor side */
		return;
	}
	struct lab_ext_workspace_group *new_grp = grp_addon->data;
	lab_transaction_add(addon->ctx, WS_PENDING_WS_ASSIGN, workspace, new_grp);
}

static void
workspace_handle_remove(struct wl_client *client, struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		/* workspace was destroyed from the compositor side */
		return;
	}
	struct lab_ext_workspace *workspace = addon->data;
	lab_transaction_add(addon->ctx, WS_PENDING_WS_REMOVE, workspace, /*argument*/ NULL);
}

static const struct ext_workspace_handle_v1_interface workspace_impl = {
	.destroy = workspace_handle_destroy,
	.activate = workspace_handle_activate,
	.deactivate = workspace_handle_deactivate,
	.assign = workspace_handle_assign,
	.remove = workspace_handle_remove,
};

static void
workspace_instance_resource_destroy(struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (addon) {
		lab_resource_addon_destroy(addon);
		wl_resource_set_user_data(resource, NULL);
	}

	wl_list_remove(wl_resource_get_link(resource));
}

static struct wl_resource *
workspace_resource_create(struct lab_ext_workspace *workspace,
		struct wl_resource *group_resource, struct lab_transaction_session_context *ctx)
{
	struct wl_client *client = wl_resource_get_client(group_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&ext_workspace_handle_v1_interface,
			wl_resource_get_version(group_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	struct lab_wl_resource_addon *addon = lab_resource_addon_create(ctx);
	addon->data = workspace;

	wl_resource_set_implementation(resource, &workspace_impl, addon,
		workspace_instance_resource_destroy);

	wl_list_insert(&workspace->resources, wl_resource_get_link(resource));
	return resource;
}

/* Workspace internal helpers */
static void
workspace_send_state(struct lab_ext_workspace *workspace, struct wl_resource *target)
{
	if (target) {
		ext_workspace_handle_v1_send_state(target, workspace->state);
	} else {
		struct wl_resource *resource;
		wl_resource_for_each(resource, &workspace->resources) {
			ext_workspace_handle_v1_send_state(resource, workspace->state);
		}
	}
}

static void
workspace_send_initial_state(struct lab_ext_workspace *workspace, struct wl_resource *resource)
{
	ext_workspace_handle_v1_send_capabilities(resource, workspace->capabilities);
	if (workspace->coordinates.size > 0) {
		ext_workspace_handle_v1_send_coordinates(resource, &workspace->coordinates);
	}
	if (workspace->name) {
		ext_workspace_handle_v1_send_name(resource, workspace->name);
	}
}

static void
workspace_set_state(struct lab_ext_workspace *workspace,
		enum ext_workspace_handle_v1_state state, bool enabled)
{
	if (!!(workspace->state_pending & state) == enabled) {
		return;
	}

	if (enabled) {
		workspace->state_pending |= state;
	} else {
		workspace->state_pending &= ~state;
	}
	ext_manager_schedule_done_event(workspace->group->manager);
}

/* Group */
static void
group_handle_transaction_destroy(struct wl_listener *listener, void *data)
{
	struct lab_transaction *transaction = data;
	wlr_log(WLR_ERROR, "destroying group transaction data %p", transaction->data);
	free(transaction->data);
}

static void
group_handle_create_workspace(struct wl_client *client,
		struct wl_resource *resource, const char *name)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		return;
	}

	struct lab_ext_workspace_group *group = addon->data;
	struct lab_transaction *transaction = lab_transaction_add(
		addon->ctx, WS_PENDING_WS_CREATE, group, xstrdup(name));
	wl_signal_add(&transaction->events.destroy, &group->on.transaction_destroy);
}

static void
group_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct ext_workspace_group_handle_v1_interface group_impl = {
	.create_workspace = group_handle_create_workspace,
	.destroy = group_handle_destroy,
};

static void
group_instance_resource_destroy(struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (addon) {
		lab_resource_addon_destroy(addon);
		wl_resource_set_user_data(resource, NULL);
	}
	wl_list_remove(wl_resource_get_link(resource));
}

static struct wl_resource *
group_resource_create(struct lab_ext_workspace_group *group,
		struct wl_resource *manager_resource, struct lab_transaction_session_context *ctx)
{
	struct wl_client *client = wl_resource_get_client(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&ext_workspace_group_handle_v1_interface,
			wl_resource_get_version(manager_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	struct lab_wl_resource_addon *addon = lab_resource_addon_create(ctx);
	addon->data = group;

	wl_resource_set_implementation(resource, &group_impl, addon,
		group_instance_resource_destroy);

	wl_list_insert(&group->resources, wl_resource_get_link(resource));
	return resource;
}

/* Group internal helpers */
static void
group_send_state(struct lab_ext_workspace_group *group, struct wl_resource *resource)
{
	ext_workspace_group_handle_v1_send_capabilities(
		resource, group->capabilities);

	ext_group_output_send_initial_state(group, resource);
}

/* Manager itself */
static void
manager_handle_commit(struct wl_client *client, struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		return;
	}

	struct lab_ext_workspace *workspace;
	struct lab_ext_workspace_group *group;
	struct lab_transaction *trans, *trans_tmp;
	wl_list_for_each_safe(trans, trans_tmp, &addon->ctx->transactions, link) {
		switch (trans->change) {
		case WS_PENDING_WS_CREATE:
			group = trans->src;
			wl_signal_emit_mutable(&group->events.create_workspace, trans->data);
			break;
		case WS_PENDING_WS_ACTIVATE:
			workspace = trans->src;
			wl_signal_emit_mutable(&workspace->events.activate, NULL);
			break;
		case WS_PENDING_WS_DEACTIVATE:
			workspace = trans->src;
			wl_signal_emit_mutable(&workspace->events.deactivate, NULL);
			break;
		case WS_PENDING_WS_REMOVE:
			workspace = trans->src;
			wl_signal_emit_mutable(&workspace->events.remove, NULL);
			break;
		case WS_PENDING_WS_ASSIGN:
			workspace = trans->src;
			wl_signal_emit_mutable(&workspace->events.assign, trans->data);
			break;
		default:
			wlr_log(WLR_ERROR, "Invalid transaction state: %u", trans->change);
		}

		lab_transaction_destroy(trans);
	}
}

static void
manager_handle_stop(struct wl_client *client, struct wl_resource *resource)
{
	ext_workspace_manager_v1_send_finished(resource);
	wl_resource_destroy(resource);
}

static const struct ext_workspace_manager_v1_interface manager_impl = {
	.commit = manager_handle_commit,
	.stop = manager_handle_stop,
};

static void
manager_instance_resource_destroy(struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (addon) {
		lab_resource_addon_destroy(addon);
		wl_resource_set_user_data(resource, NULL);
	}

	wl_list_remove(wl_resource_get_link(resource));
}

static void
manager_handle_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id)
{
	struct lab_ext_workspace_manager *manager = data;
	struct wl_resource *resource = wl_resource_create(client,
			&ext_workspace_manager_v1_interface,
			version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	struct lab_wl_resource_addon *addon =
		lab_resource_addon_create(/* session context*/ NULL);
	addon->data = manager;

	wl_resource_set_implementation(resource, &manager_impl,
		addon, manager_instance_resource_destroy);

	wl_list_insert(&manager->resources, wl_resource_get_link(resource));

	struct lab_ext_workspace *workspace;
	struct lab_ext_workspace_group *group;
	wl_list_for_each(group, &manager->groups, link) {
		/* Create group resource */
		struct wl_resource *group_resource =
			group_resource_create(group, resource, addon->ctx);
		ext_workspace_manager_v1_send_workspace_group(resource, group_resource);
		group_send_state(group, group_resource);

		/* Create workspace resource */
		wl_list_for_each(workspace, &group->workspaces, link) {
			struct wl_resource *workspace_resource =
				workspace_resource_create(workspace, group_resource, addon->ctx);
			ext_workspace_manager_v1_send_workspace(resource, workspace_resource);
			ext_workspace_group_handle_v1_send_workspace_enter(
				group_resource, workspace_resource);
			workspace_send_initial_state(workspace, workspace_resource);
			/* Send the current workspace state to this client only */
			workspace_send_state(workspace, workspace_resource);
		}
	}
	ext_workspace_manager_v1_send_done(resource);
}

static void
manager_handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct lab_ext_workspace_manager *manager =
		wl_container_of(listener, manager, on.display_destroy);

	struct lab_ext_workspace_group *group, *tmp;
	wl_list_for_each_safe(group, tmp, &manager->groups, link) {
		lab_ext_workspace_group_destroy(group);
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
	struct lab_ext_workspace_manager *manager = data;

	struct lab_ext_workspace *workspace;
	struct lab_ext_workspace_group *group;
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
		ext_workspace_manager_v1_send_done(resource);
	}
	manager->idle_source = NULL;
}

/* Internal API */
void
ext_manager_schedule_done_event(struct lab_ext_workspace_manager *manager)
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
struct lab_ext_workspace_manager *
lab_ext_workspace_manager_create(struct wl_display *display, uint32_t caps, uint32_t version)
{
	assert(version <= EXT_WORKSPACE_V1_VERSION);

	struct lab_ext_workspace_manager *manager = znew(*manager);
	manager->global = wl_global_create(display,
			&ext_workspace_manager_v1_interface,
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

struct lab_ext_workspace_group *
lab_ext_workspace_group_create(struct lab_ext_workspace_manager *manager)
{
	assert(manager);

	struct lab_ext_workspace_group *group = znew(*group);
	group->manager = manager;
	group->capabilities = manager->caps & WS_CAP_GRP_ALL;

	wl_list_init(&group->outputs);
	wl_list_init(&group->resources);
	wl_list_init(&group->workspaces);
	wl_signal_init(&group->events.create_workspace);
	wl_signal_init(&group->events.destroy);

	group->on.transaction_destroy.notify = group_handle_transaction_destroy;

	wl_list_append(&manager->groups, &group->link);

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &manager->resources) {
		struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
		assert(addon && addon->ctx);
		struct wl_resource *group_resource =
			group_resource_create(group, resource, addon->ctx);
		ext_workspace_manager_v1_send_workspace_group(resource, group_resource);
		group_send_state(group, group_resource);
	}
	ext_manager_schedule_done_event(manager);

	return group;
}

void
lab_ext_workspace_group_destroy(struct lab_ext_workspace_group *group)
{
	if (!group) {
		return;
	}
	wl_signal_emit_mutable(&group->events.destroy, NULL);

	struct lab_ext_workspace *ws, *ws_tmp;
	wl_list_for_each_safe(ws, ws_tmp, &group->workspaces, link) {
		lab_ext_workspace_destroy(ws);
	}

	struct wl_resource *resource, *res_tmp;
	wl_resource_for_each_safe(resource, res_tmp, &group->resources) {
		struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
		if (addon) {
			lab_resource_addon_destroy(addon);
			wl_resource_set_user_data(resource, NULL);
		}
		ext_workspace_group_handle_v1_send_removed(resource);
		wl_list_remove(wl_resource_get_link(resource));
		wl_list_init(wl_resource_get_link(resource));
	}

	wl_list_remove(&group->link);
	free(group);
}

struct lab_ext_workspace *
lab_ext_workspace_create(struct lab_ext_workspace_group *group)
{
	assert(group);

	struct lab_ext_workspace *workspace = znew(*workspace);
	workspace->group = group;
	/*
	 * Ensures we are sending workspace->state_pending on the done event,
	 * regardless if the compositor has changed any state in between here
	 * and the scheduled done event or not.
	 *
	 * Without this we might have to send the state twice, first here and
	 * then again in the scheduled done event when there were any changes.
	 */
	workspace->state = WS_STATE_INVALID;
	workspace->capabilities = (group->manager->caps & WS_CAP_WS_ALL) >> 16;

	wl_list_init(&workspace->resources);
	wl_array_init(&workspace->coordinates);
	wl_signal_init(&workspace->events.activate);
	wl_signal_init(&workspace->events.deactivate);
	wl_signal_init(&workspace->events.remove);
	wl_signal_init(&workspace->events.assign);
	wl_signal_init(&workspace->events.destroy);

	wl_list_append(&group->workspaces, &workspace->link);

	/* Notify clients */
	struct wl_resource *group_resource, *workspace_resource, *manager_resource;
	struct lab_wl_resource_addon *group_addon, *manager_addon;
	wl_resource_for_each(group_resource, &group->resources) {
		group_addon = wl_resource_get_user_data(group_resource);
		// FIXME: verify we can either user assert() or fail gracefully everywhere
		assert(group_addon && group_addon->ctx);

		workspace_resource = workspace_resource_create(
			workspace, group_resource, group_addon->ctx);

		wl_resource_for_each(manager_resource, &group->manager->resources) {
			manager_addon = wl_resource_get_user_data(manager_resource);
			if (!manager_addon || manager_addon->ctx != group_addon->ctx) {
				continue;
			}
			ext_workspace_manager_v1_send_workspace(
				manager_resource, workspace_resource);
			break;
		}

		ext_workspace_group_handle_v1_send_workspace_enter(
			group_resource, workspace_resource);
		workspace_send_initial_state(workspace, workspace_resource);
	}
	ext_manager_schedule_done_event(group->manager);

	return workspace;
}

void
lab_ext_workspace_set_name(struct lab_ext_workspace *workspace, const char *name)
{
	assert(workspace);
	assert(name);

	if (!workspace->name || strcmp(workspace->name, name)) {
		free(workspace->name);
		workspace->name = xstrdup(name);
		struct wl_resource *resource;
		wl_resource_for_each(resource, &workspace->resources) {
			ext_workspace_handle_v1_send_name(resource, workspace->name);
		}
	}
	ext_manager_schedule_done_event(workspace->group->manager);
}

void
lab_ext_workspace_set_active(struct lab_ext_workspace *workspace, bool enabled)
{
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE, enabled);
}

void
lab_ext_workspace_set_urgent(struct lab_ext_workspace *workspace, bool enabled)
{
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_URGENT, enabled);
}

void
lab_ext_workspace_set_hidden(struct lab_ext_workspace *workspace, bool enabled)
{
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN, enabled);
}

void
lab_ext_workspace_set_coordinates(struct lab_ext_workspace *workspace,
		struct wl_array *coordinates)
{
	wl_array_release(&workspace->coordinates);
	wl_array_init(&workspace->coordinates);
	wl_array_copy(&workspace->coordinates, coordinates);

	struct wl_resource *resource;
	wl_resource_for_each(resource, &workspace->resources) {
		ext_workspace_handle_v1_send_coordinates(resource, &workspace->coordinates);
	}
	ext_manager_schedule_done_event(workspace->group->manager);
}

void
lab_ext_workspace_destroy(struct lab_ext_workspace *workspace)
{
	if (!workspace) {
		return;
	}
	wl_signal_emit_mutable(&workspace->events.destroy, NULL);

	struct wl_resource *ws_res, *ws_tmp, *grp_res;
	struct lab_wl_resource_addon *ws_addon, *grp_addon;
	wl_resource_for_each_safe(ws_res, ws_tmp, &workspace->resources) {
		ws_addon = wl_resource_get_user_data(ws_res);

		if (ws_addon) {
			/* Notify group about workspace leaving */
			wl_resource_for_each(grp_res, &workspace->group->resources) {
				grp_addon = wl_resource_get_user_data(grp_res);
				if (!grp_addon || grp_addon->ctx != ws_addon->ctx) {
					continue;
				}
				ext_workspace_group_handle_v1_send_workspace_leave(
					grp_res, ws_res);
				break;
			}
			lab_resource_addon_destroy(ws_addon);
			wl_resource_set_user_data(ws_res, NULL);
		}
		ext_workspace_handle_v1_send_removed(ws_res);
		wl_list_remove(wl_resource_get_link(ws_res));
		wl_list_init(wl_resource_get_link(ws_res));
	}
	ext_manager_schedule_done_event(workspace->group->manager);

	wl_list_remove(&workspace->link);
	wl_array_release(&workspace->coordinates);
	zfree(workspace->name);
	free(workspace);
}
