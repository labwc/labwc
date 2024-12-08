/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_TRANSACTION_ADDON_H
#define LABWC_PROTOCOLS_TRANSACTION_ADDON_H

#include <wayland-server-core.h>

struct lab_transaction {
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
	struct wl_list transactions;
};

struct lab_wl_resource_addon {
	struct lab_transaction_session_context *ctx;
	void *data;
};

struct lab_wl_resource_addon *lab_resource_addon_create(
	struct lab_transaction_session_context *ctx);

struct lab_transaction *lab_transaction_add(struct lab_transaction_session_context *ctx,
	uint32_t pending_change, void *src, void *argument);

void lab_transaction_destroy(struct lab_transaction *transaction);

void lab_resource_addon_destroy(struct lab_wl_resource_addon *addon);

#endif /* LABWC_PROTOCOLS_TRANSACTIONS_ADDON_H */
