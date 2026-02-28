// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "permissions.h"
#include <glib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "common/spawn.h"
#include "config/rcxml.h"

struct permissions_context {
	uint32_t permissions;
	int fd;
	struct wl_listener destroy;
};

static void
permissions_context_handle_destroy(struct wl_listener *listener, void *data)
{
	struct permissions_context *context = wl_container_of(listener, context, destroy);
	close(context->fd);
	zfree(context);
}

static uint32_t
permissions_get(const struct wl_client *client)
{
	struct wl_listener *listener = wl_client_get_destroy_listener(
		(struct wl_client *)client, permissions_context_handle_destroy);
	if (!listener) {
		return 0;
	}

	struct permissions_context *context = wl_container_of(listener, context, destroy);
	return context->permissions;
}

static bool
context_create(struct wl_display *display, uint32_t permissions, int fd)
{
	struct wl_client *client = wl_client_create(display, fd);
	if (!client) {
		wlr_log(WLR_ERROR, "failed to create client");
		return false;
	}

	struct permissions_context *context = znew(*context);
	context->permissions = permissions;
	context->fd = fd;
	context->destroy.notify = permissions_context_handle_destroy;
	wl_client_add_destroy_listener(client, &context->destroy);

	return true;
}

int
permissions_context_create(struct wl_display *display, uint32_t permissions)
{
	if (permissions == 0) {
		return -1;
	}

	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
		wlr_log_errno(WLR_ERROR, "socketpair failed");
		return -1;
	}

	if (!set_cloexec(sv[0])) {
		close(sv[0]);
		close(sv[1]);
		return -1;
	}

	if (!context_create(display, permissions, sv[0])) {
		close(sv[0]);
		close(sv[1]);
		return -1;
	}

	return sv[1];
}

bool
permissions_context_clone(struct wl_client *client, int fd)
{
	struct wl_display *display = wl_client_get_display(client);
	uint32_t permissions = permissions_get(client);
	if (permissions == 0) {
		return false;
	}
	return context_create(display, permissions, fd);
}

bool
permissions_check(const struct wl_client *client, const struct wl_interface *iface)
{
	uint32_t permissions = permissions_get(client) | rc.allowed_interfaces;
	uint32_t required = parse_privileged_interface(iface->name);
	return (permissions & required) == required;
}
