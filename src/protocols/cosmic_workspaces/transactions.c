// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "protocols/cosmic-workspaces-internal.h"

static void
transactions_destroy(struct wl_list *list)
{
	struct transaction_group *group;
	struct transaction *trans, *trans_tmp;
	wl_list_for_each_safe(trans, trans_tmp, list, link) {
		if (trans->change == CW_PENDING_WS_CREATE) {
			group = wl_container_of(trans, group, base);
			free(group->new_workspace_name);
		}
		wl_list_remove(&trans->link);
		free(trans);
	}
}

void
resource_addon_destroy(struct wl_resource_addon *addon)
{
	assert(addon);
	assert(addon->ctx);

	addon->ctx->ref_count--;
	assert(addon->ctx->ref_count >= 0);

	wlr_log(WLR_DEBUG, "New refcount for session %p: %d",
		addon->ctx, addon->ctx->ref_count);
	if (!addon->ctx->ref_count) {
		wlr_log(WLR_DEBUG, "Destroying session context");
		transactions_destroy(&addon->ctx->transactions);
		free(addon->ctx);
	}

	free(addon);
}

struct wl_resource_addon *
resource_addon_create(struct session_context *ctx)
{
	struct wl_resource_addon *addon = znew(*addon);
	if (!ctx) {
		ctx = znew(*ctx);
		wl_list_init(&ctx->transactions);
	}
	addon->ctx = ctx;
	addon->ctx->ref_count++;
	return addon;
}

void
transaction_add_workspace_ev(struct lab_cosmic_workspace *ws,
		struct wl_resource *resource, enum pending_change change)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		wlr_log(WLR_ERROR, "Failed to find manager addon for workspace transaction");
		return;
	}

	assert(change != CW_PENDING_WS_CREATE);

	struct transaction_workspace *trans_ws = znew(*trans_ws);
	trans_ws->workspace = ws;
	trans_ws->base.change = change;
	wl_list_append(&addon->ctx->transactions, &trans_ws->base.link);
}

void
transaction_add_workspace_group_ev(struct lab_cosmic_workspace_group *group,
		struct wl_resource *resource, enum pending_change change,
		const char *new_workspace_name)
{
	struct wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (!addon) {
		wlr_log(WLR_ERROR, "Failed to find manager addon for group transaction");
		return;
	}

	assert(change == CW_PENDING_WS_CREATE);

	struct transaction_group *trans_grp = znew(*trans_grp);
	trans_grp->group = group;
	trans_grp->base.change = change;
	trans_grp->new_workspace_name = xstrdup(new_workspace_name);
	wl_list_append(&addon->ctx->transactions, &trans_grp->base.link);
}
