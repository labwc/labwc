// SPDX-License-Identifier: GPL-2.0-only
#include <stdlib.h>
#include <protocols/ext_socket_manager_v1.h>
#include "permissions.h"
#include "ext-socket-manager-unstable-v1-protocol.h"

#define EXT_SOCKET_MANAGER_V1_VERSION 1

static void
manager_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
manager_handle_register_socket(struct wl_client *client,
		struct wl_resource *manager_resource, int fd)
{
	permissions_context_clone(client, fd);
}

static const struct ext_socket_manager_v1_interface manager_impl = {
	.destroy = manager_handle_destroy,
	.register_socket = manager_handle_register_socket,
};

static void
manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct ext_socket_manager_v1 *manager = data;

	struct wl_resource *resource =
		wl_resource_create(client, &ext_socket_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

static void
handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct ext_socket_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, manager);

	wl_global_destroy(manager->global);
	wl_list_remove(&manager->display_destroy.link);
	free(manager);
}

struct ext_socket_manager_v1 *
ext_socket_manager_v1_create(struct wl_display *display)
{
	struct ext_socket_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (!manager) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&ext_socket_manager_v1_interface,
		EXT_SOCKET_MANAGER_V1_VERSION, manager, manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.register_socket);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
