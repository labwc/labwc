// SPDX-License-Identifier: GPL-2.0-only
#include <sys/socket.h>
#include <sys/un.h>
#include <wayland-server-core.h>
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "server-unpriv.h"

static struct unpriv_socket {
	char *path;
	int listen_fd;
	struct wl_event_source *event_source;
	struct wl_listener on_display_destroy;

	struct wl_list clients;
} unpriv_socket;

struct unpriv_client {
	struct wl_client *wl_client;
	struct wl_listener on_destroy;

	struct wl_list link;
};

static void
unpriv_socket_finish(void)
{
	if (unpriv_socket.listen_fd > 0) {
		close(unpriv_socket.listen_fd);
		unpriv_socket.listen_fd = 0;
	}
	if (unpriv_socket.event_source) {
		wl_event_source_remove(unpriv_socket.event_source);
		unpriv_socket.event_source = NULL;
	}
	if (unpriv_socket.path) {
		unlink(unpriv_socket.path);
		zfree(unpriv_socket.path);
	}
}

static void
on_unpriv_client_destroy(struct wl_listener *listener, void *data)
{
	struct unpriv_client *client = wl_container_of(listener, client, on_destroy);
	wl_list_remove(&client->on_destroy.link);
	wl_list_remove(&client->link);
	free(client);
	wlr_log(WLR_DEBUG, "unpriv client destroyed");
}

static int
on_unpriv_socket_connect(int fd, uint32_t mask, void *data)
{
	struct wl_display *display = data;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		wlr_log(WLR_DEBUG, "unpriv socket going down, cleaning up");
		unpriv_socket_finish();
		return 0;
	}

	/* FIXME: cloexec / accept4 */
	if (mask & WL_EVENT_READABLE) {
		int client_fd = accept(fd, NULL, NULL);
		if (client_fd < 0) {
			wlr_log(WLR_ERROR, "Failed to accept unpriv client");
			return 0;
		};

		struct unpriv_client *client = znew(*client);
		client->wl_client = wl_client_create(display, client_fd);
		if (!client->wl_client) {
			wlr_log(WLR_ERROR, "Failed to create unpriv client");
			close(client_fd);
			free(client);
			return 0;
		}

		wlr_log(WLR_DEBUG, "Accepted new unpriv client: %i", client_fd);
		wl_list_append(&unpriv_socket.clients, &client->link);
		client->on_destroy.notify = on_unpriv_client_destroy;
		wl_client_add_destroy_listener(client->wl_client, &client->on_destroy);
	}
	return 0;
}

static void
on_display_destroy(struct wl_listener *listener, void *data)
{
	wl_list_remove(&unpriv_socket.on_display_destroy.link);
	unpriv_socket_finish();
}

void
unpriv_socket_start(struct server *server)
{
	wl_list_init(&unpriv_socket.clients);

	/* FIXME: lockfile, CLOEXEC fallback */

	struct sockaddr_un addr = { .sun_family = AF_LOCAL };
	int addr_size = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s",
		getenv("XDG_RUNTIME_DIR"), "wayland-unpriv") + 1;

	if (addr_size > (int)sizeof(addr.sun_path)) {
		addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
		wlr_log(WLR_ERROR, "Truncated unpriv socket path to %s", addr.sun_path);
	}

	/*
	 * TODO: use close-on-exec fallback like
	 * https://gitlab.freedesktop.org/wayland/wayland/-/blob/main/src/wayland-os.c#L74
	 */
	int fd = socket(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		wlr_log(WLR_ERROR, "Failed to create unpriv socket");
		return;
	}
	unpriv_socket.listen_fd = fd;
	wlr_log(WLR_ERROR, "Got unpriv fd: %i", fd);

	/* TODO: why offsetof(addr, sun_path) + strlen(addr.sun_path) ? */
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		wlr_log(WLR_ERROR, "Failed to bind unpriv socket");
		goto error;
	}
	unpriv_socket.path = xstrdup(addr.sun_path);

	/* A Backlog of 16 seems more than enough for a unprivileged socket */
	if (listen(fd, 16) < 0) {
		wlr_log(WLR_ERROR, "Failed to listen in unpriv socket");
		goto error;
	}

	unpriv_socket.event_source = wl_event_loop_add_fd(
		server->wl_event_loop, fd, WL_EVENT_READABLE,
		on_unpriv_socket_connect, server->wl_display);
	if (!unpriv_socket.event_source) {
		wlr_log(WLR_ERROR, "Failed to add unpriv socket to loop");
		goto error;
	}

	unpriv_socket.on_display_destroy.notify = on_display_destroy;
	wl_display_add_destroy_listener(server->wl_display, &unpriv_socket.on_display_destroy);

	wlr_log(WLR_INFO, "Unprivileged socket opened at '%s'", addr.sun_path);
	return;

error:
	unpriv_socket_finish();
}

bool
is_unpriv_client(const struct wl_client *wl_client)
{
	if (unpriv_socket.listen_fd <= 0) {
		/* TODO: we could return true here to block the protocols for the main socket */
		return false;
	}
	struct unpriv_client *unpriv_client;
	wl_list_for_each(unpriv_client, &unpriv_socket.clients, link) {
		if (wl_client == unpriv_client->wl_client) {
			return true;
		}
	}
	return false;
}
