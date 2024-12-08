// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "protocols/transaction-addon.h"

void
lab_transaction_destroy(struct lab_transaction *transaction)
{
	wl_signal_emit_mutable(&transaction->events.destroy, transaction);
	wl_list_remove(&transaction->link);
	free(transaction);
}

static void
transactions_destroy(struct wl_list *list)
{
	struct lab_transaction *trans, *trans_tmp;
	wl_list_for_each_safe(trans, trans_tmp, list, link) {
		lab_transaction_destroy(trans);
	}
}

void
lab_resource_addon_destroy(struct lab_wl_resource_addon *addon)
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

struct lab_wl_resource_addon *
lab_resource_addon_create(struct lab_transaction_session_context *ctx)
{
	struct lab_wl_resource_addon *addon = znew(*addon);
	if (!ctx) {
		ctx = znew(*ctx);
		wl_list_init(&ctx->transactions);
	}
	addon->ctx = ctx;
	addon->ctx->ref_count++;
	return addon;
}

struct lab_transaction *
lab_transaction_add(struct lab_transaction_session_context *ctx,
		uint32_t pending_change, void *src, void *argument)
{
	assert(ctx);

	struct lab_transaction *transaction = znew(*transaction);
	transaction->change = pending_change;
	transaction->src = src;
	transaction->data = argument;

	wl_signal_init(&transaction->events.destroy);
	wl_list_append(&ctx->transactions, &transaction->link);

	return transaction;
}
