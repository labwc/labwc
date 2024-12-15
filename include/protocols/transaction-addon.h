/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_TRANSACTION_ADDON_H
#define LABWC_PROTOCOLS_TRANSACTION_ADDON_H

#include <wayland-server-core.h>

struct lab_transaction_op {
	uint32_t change;
	void *src;
	void *data;

	struct {
		struct wl_signal destroy;
	} events;

	// Private
	struct wl_list link;
};

struct lab_transaction_session_context {
	int ref_count;
	struct wl_list transaction_ops;
};

struct lab_wl_resource_addon {
	struct lab_transaction_session_context *ctx;
	void *data;
};

/*
 * Creates a new addon which can be attached to a wl_resource via
 * wl_resource_set_user_data() and retrieved via wl_resource_get_user_data().
 *
 * Usually the ctx argument should be addon->ctx of the parent wl_resource.
 * If it is NULL it will be created automatically which can be used for top
 * level wl_resources (when a client binds a wl_global from the registry).
 *
 * The context refcount is increased by one after this call.
 */
struct lab_wl_resource_addon *lab_resource_addon_create(
	struct lab_transaction_session_context *ctx);

/*
 * A generic transaction operation attached to
 * a session context transaction operation list.
 *
 * All arguments other than the context are user defined.
 * Use of an enum for pending_change is suggested.
 *
 * The client is responsible for eventually freeing the data
 * passed in the void *src and *data arguments by listening
 * to the events.destroy signal. The transaction operations can be
 * looped through by using lab_transaction_for_each(trans_op, ctx).
 */
struct lab_transaction_op *lab_transaction_op_add(
	struct lab_transaction_session_context *ctx,
	uint32_t pending_change, void *src, void *data);

/*
 * Removes the transaction operation from the ctx list and frees it.
 *
 * Does *not* free any passed in src or data arguments.
 * Use the events.destroy signal for that if necessary.
 */
void lab_transaction_op_destroy(struct lab_transaction_op *transaction_op);

/*
 * Destroys the addon.
 *
 * The context refcount is decreased by one. If it reaches
 * zero the context will be free'd alongside the addon itself.
 * If the context is destroyed all pending transaction operations
 * are destroyed as well.
 */
void lab_resource_addon_destroy(struct lab_wl_resource_addon *addon);

/* Convinience wrappers for looping through the pending transaction ops of a ctx */
#define lab_transaction_for_each(transaction_op, ctx) \
	wl_list_for_each(transaction_op, &(ctx)->transaction_ops, link)

#define lab_transaction_for_each_safe(trans_op, trans_op_tmp, ctx) \
	wl_list_for_each_safe(trans_op, trans_op_tmp, &(ctx)->transaction_ops, link)

#endif /* LABWC_PROTOCOLS_TRANSACTIONS_ADDON_H */
