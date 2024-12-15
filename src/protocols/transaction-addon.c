// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wayland-server-core.h>
#include "common/list.h"
#include "common/mem.h"
#include "protocols/transaction-addon.h"

void
lab_transaction_op_destroy(struct lab_transaction_op *trans_op)
{
	wl_signal_emit_mutable(&trans_op->events.destroy, trans_op);
	wl_list_remove(&trans_op->link);
	free(trans_op);
}

static void
transaction_destroy(struct wl_list *list)
{
	struct lab_transaction_op *trans_op, *trans_op_tmp;
	wl_list_for_each_safe(trans_op, trans_op_tmp, list, link) {
		lab_transaction_op_destroy(trans_op);
	}
}

void
lab_resource_addon_destroy(struct lab_wl_resource_addon *addon)
{
	assert(addon);
	assert(addon->ctx);

	addon->ctx->ref_count--;
	assert(addon->ctx->ref_count >= 0);

	if (!addon->ctx->ref_count) {
		transaction_destroy(&addon->ctx->transaction_ops);
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
		wl_list_init(&ctx->transaction_ops);
	}
	addon->ctx = ctx;
	addon->ctx->ref_count++;
	return addon;
}

struct lab_transaction_op *
lab_transaction_op_add(struct lab_transaction_session_context *ctx,
		uint32_t pending_change, void *src, void *data)
{
	assert(ctx);

	struct lab_transaction_op *trans_op = znew(*trans_op);
	trans_op->change = pending_change;
	trans_op->src = src;
	trans_op->data = data;

	wl_signal_init(&trans_op->events.destroy);
	wl_list_append(&ctx->transaction_ops, &trans_op->link);

	return trans_op;
}
