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

enum lab_permission {
	LAB_PERM_DRM_LEASE        = 1 << 0,
	LAB_PERM_GAMMA            = 1 << 1,
	LAB_PERM_OUTPUT           = 1 << 2,
	LAB_PERM_OUTPUT_POWER     = 1 << 3,
	LAB_PERM_INPUT_METHOD     = 1 << 4,
	LAB_PERM_VIRTUAL_POINTER  = 1 << 5,
	LAB_PERM_VIRTUAL_KEYBOARD = 1 << 6,
	LAB_PERM_DMABUF           = 1 << 7,
	LAB_PERM_SCREENCOPY       = 1 << 8,
	LAB_PERM_DATA_CONTROL     = 1 << 9,
	LAB_PERM_IDLE_NOTIFIER    = 1 << 10,
	LAB_PERM_WORKSPACE        = 1 << 11,
	LAB_PERM_FOREIGN_TOPLEVEL = 1 << 12,
	LAB_PERM_SESSION_LOCK     = 1 << 13,
	LAB_PERM_LAYER_SHELL      = 1 << 14,
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

uint32_t
permissions_from_interface_name(const char *name)
{
	if (!strcmp(name, "wp_drm_lease_device_v1")) {
		return LAB_PERM_DRM_LEASE;
	} else if (!strcmp(name, "zwlr_gamma_control_manager_v1")) {
		return LAB_PERM_GAMMA;
	} else if (!strcmp(name, "zwlr_output_manager_v1")) {
		return LAB_PERM_OUTPUT;
	} else if (!strcmp(name, "zwlr_output_power_manager_v1")) {
		return LAB_PERM_OUTPUT_POWER;
	} else if (!strcmp(name, "zwp_input_method_manager_v2")) {
		return LAB_PERM_INPUT_METHOD;
	} else if (!strcmp(name, "zwlr_virtual_pointer_manager_v1")) {
		return LAB_PERM_VIRTUAL_POINTER;
	} else if (!strcmp(name, "zwp_virtual_keyboard_manager_v1")) {
		return LAB_PERM_VIRTUAL_KEYBOARD;
	} else if (!strcmp(name, "zwlr_export_dmabuf_manager_v1")) {
		return LAB_PERM_DMABUF;
	} else if (!strcmp(name, "zwlr_screencopy_manager_v1")) {
		return LAB_PERM_SCREENCOPY;
	} else if (!strcmp(name, "zwlr_data_control_manager_v1")) {
		return LAB_PERM_DATA_CONTROL;
	} else if (!strcmp(name, "ext_idle_notifier_v1")) {
		return LAB_PERM_IDLE_NOTIFIER;
	} else if (!strcmp(name, "zcosmic_workspace_manager_v1")) {
		return LAB_PERM_WORKSPACE;
	} else if (!strcmp(name, "zwlr_foreign_toplevel_manager_v1")) {
		return LAB_PERM_FOREIGN_TOPLEVEL;
	} else if (!strcmp(name, "ext_foreign_toplevel_list_v1")) {
		return LAB_PERM_FOREIGN_TOPLEVEL;
	} else if (!strcmp(name, "ext_session_lock_manager_v1")) {
		return LAB_PERM_SESSION_LOCK;
	} else if (!strcmp(name, "zwlr_layer_shell_v1")) {
		return LAB_PERM_LAYER_SHELL;
	} else {
		return 0;
	}
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

	struct wl_client *client = wl_client_create(display, sv[0]);
	if (!client) {
		wlr_log(WLR_ERROR, "failed to create client");
		close(sv[0]);
		close(sv[1]);
		return -1;
	}

	struct permissions_context *context = znew(*context);
	context->permissions = permissions;
	context->fd = sv[0];
	context->destroy.notify = permissions_context_handle_destroy;
	wl_client_add_destroy_listener(client, &context->destroy);

	return sv[1];
}

bool
permissions_check(const struct wl_client *client, const struct wl_interface *iface)
{
	uint32_t permissions = permissions_get(client);
	uint32_t required = permissions_from_interface_name(iface->name);
	return (permissions & required) == required;
}
