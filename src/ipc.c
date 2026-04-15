// SPDX-License-Identifier: GPL-2.0-only
/*
 * ipc.c - labwc IPC server (sway-compatible protocol)
 *
 * Implements:
 *   - UNIX domain socket with "labwc-ipc" wire protocol
 *   - All message types 0-12 and 100-101
 *   - Core events: workspace, output, window, shutdown, tick
 *   - Sway-compatible command parser for RUN_COMMAND
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <libinput.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "config.h"

#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include "action.h"
#include "common/buf.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "foreign-toplevel/foreign.h"
#include "input/input.h"
#include "ipc.h"
#include "labwc.h"
#include "output.h"
#include "view.h"
#include "workspaces.h"

/* ===================================================================
 * Section 1: Protocol & Client Management
 * =================================================================== */

static int ipc_socket_fd = -1;
static char *ipc_socket_path;
static struct wl_event_source *ipc_event_source;
static struct wl_list ipc_clients;


static void ipc_client_disconnect(struct ipc_client *client);
static void ipc_handle_message(struct ipc_client *client, uint32_t type,
	const char *payload, uint32_t len);

static void
ipc_send_reply(struct ipc_client *client, uint32_t type, const char *payload,
	uint32_t len)
{
	char header[IPC_HEADER_SIZE];
	memcpy(header, IPC_MAGIC, IPC_MAGIC_LEN);
	memcpy(header + IPC_MAGIC_LEN, &len, sizeof(uint32_t));
	memcpy(header + IPC_MAGIC_LEN + 4, &type, sizeof(uint32_t));

	size_t total = IPC_HEADER_SIZE + len;
	size_t old_len = client->write_buf_len;
	size_t new_len = old_len + total;

	if (new_len > client->write_buf_cap) {
		size_t new_cap = new_len * 2;
		if (new_cap < 4096) {
			new_cap = 4096;
		}
		client->write_buf = realloc(client->write_buf, new_cap);
		client->write_buf_cap = new_cap;
	}
	memcpy(client->write_buf + old_len, header, IPC_HEADER_SIZE);
	if (len > 0) {
		memcpy(client->write_buf + old_len + IPC_HEADER_SIZE, payload,
			len);
	}
	client->write_buf_len = new_len;

	/* Try to flush immediately */
	while (client->write_buf_len > 0) {
		ssize_t n = write(client->fd, client->write_buf,
			client->write_buf_len);
		if (n < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				break;
			}
			ipc_client_disconnect(client);
			return;
		}
		memmove(client->write_buf, client->write_buf + n,
			client->write_buf_len - n);
		client->write_buf_len -= n;
	}
}

static void
ipc_send_reply_json(struct ipc_client *client, uint32_t type,
	struct json_object *obj)
{
	const char *str = json_object_to_json_string(obj);
	ipc_send_reply(client, type, str, strlen(str));
	json_object_put(obj);
}

static int
handle_client_readable(int fd, uint32_t mask, void *data)
{
	struct ipc_client *client = data;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		ipc_client_disconnect(client);
		return 0;
	}

	/* Read header */
	char header[IPC_HEADER_SIZE];
	size_t total_read = 0;
	while (total_read < IPC_HEADER_SIZE) {
		ssize_t n = read(fd, header + total_read,
			IPC_HEADER_SIZE - total_read);
		if (n <= 0) {
			ipc_client_disconnect(client);
			return 0;
		}
		total_read += n;
	}

	/* Validate magic */
	if (memcmp(header, IPC_MAGIC, IPC_MAGIC_LEN) != 0) {
		wlr_log(WLR_ERROR, "IPC: invalid magic from client");
		ipc_client_disconnect(client);
		return 0;
	}

	uint32_t payload_len, msg_type;
	memcpy(&payload_len, header + IPC_MAGIC_LEN, sizeof(uint32_t));
	memcpy(&msg_type, header + IPC_MAGIC_LEN + 4, sizeof(uint32_t));

	/* Sanity limit: 1MB */
	if (payload_len > 1024 * 1024) {
		wlr_log(WLR_ERROR, "IPC: message too large (%u bytes)",
			payload_len);
		ipc_client_disconnect(client);
		return 0;
	}

	char *payload = NULL;
	if (payload_len > 0) {
		payload = malloc(payload_len + 1);
		if (!payload) {
			ipc_client_disconnect(client);
			return 0;
		}
		size_t read_so_far = 0;
		while (read_so_far < payload_len) {
			ssize_t n = read(fd, payload + read_so_far,
				payload_len - read_so_far);
			if (n <= 0) {
				free(payload);
				ipc_client_disconnect(client);
				return 0;
			}
			read_so_far += n;
		}
		payload[payload_len] = '\0';
	}

	ipc_handle_message(client, msg_type, payload, payload_len);
	free(payload);
	return 0;
}

static int
handle_new_connection(int fd, uint32_t mask, void *data)
{
	int client_fd = accept(fd, NULL, NULL);
	if (client_fd < 0) {
		wlr_log_errno(WLR_ERROR, "IPC: accept failed");
		return 0;
	}

	int flags = fcntl(client_fd, F_GETFD);
	if (flags >= 0) {
		fcntl(client_fd, F_SETFD, flags | FD_CLOEXEC);
	}

	struct ipc_client *client = znew(*client);
	client->fd = client_fd;
	client->readable = wl_event_loop_add_fd(server.wl_event_loop, client_fd,
		WL_EVENT_READABLE, handle_client_readable, client);
	wl_list_insert(&ipc_clients, &client->link);

	return 0;
}

static void
ipc_client_disconnect(struct ipc_client *client)
{
	wl_event_source_remove(client->readable);
	if (client->writable) {
		wl_event_source_remove(client->writable);
	}
	close(client->fd);
	wl_list_remove(&client->link);
	free(client->write_buf);
	free(client);
}

void
ipc_init(void)
{
	wl_list_init(&ipc_clients);

	ipc_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc_socket_fd < 0) {
		wlr_log_errno(WLR_ERROR, "IPC: failed to create socket");
		return;
	}

	int flags = fcntl(ipc_socket_fd, F_GETFD);
	if (flags >= 0) {
		fcntl(ipc_socket_fd, F_SETFD, flags | FD_CLOEXEC);
	}

	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wlr_log(WLR_ERROR, "IPC: XDG_RUNTIME_DIR not set");
		close(ipc_socket_fd);
		ipc_socket_fd = -1;
		return;
	}

	int path_len = snprintf(NULL, 0, "%s/labwc-ipc.%d.sock", runtime_dir,
		getpid());
	ipc_socket_path = malloc(path_len + 1);
	snprintf(ipc_socket_path, path_len + 1, "%s/labwc-ipc.%d.sock",
		runtime_dir, getpid());

	/* Remove stale socket */
	unlink(ipc_socket_path);

	struct sockaddr_un addr = {0};
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ipc_socket_path, sizeof(addr.sun_path) - 1);

	if (bind(ipc_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		wlr_log_errno(WLR_ERROR, "IPC: bind failed on %s",
			ipc_socket_path);
		close(ipc_socket_fd);
		ipc_socket_fd = -1;
		free(ipc_socket_path);
		ipc_socket_path = NULL;
		return;
	}

	chmod(ipc_socket_path, 0600);

	if (listen(ipc_socket_fd, 16) < 0) {
		wlr_log_errno(WLR_ERROR, "IPC: listen failed");
		close(ipc_socket_fd);
		ipc_socket_fd = -1;
		unlink(ipc_socket_path);
		free(ipc_socket_path);
		ipc_socket_path = NULL;
		return;
	}

	ipc_event_source = wl_event_loop_add_fd(server.wl_event_loop,
		ipc_socket_fd, WL_EVENT_READABLE, handle_new_connection, NULL);

	setenv("LABWC_IPC_SOCK", ipc_socket_path, 1);
	wlr_log(WLR_INFO, "IPC: listening on %s", ipc_socket_path);
}

void
ipc_finish(void)
{
	if (ipc_socket_fd < 0) {
		return;
	}

	struct ipc_client *client, *tmp;
	wl_list_for_each_safe(client, tmp, &ipc_clients, link) {
		ipc_client_disconnect(client);
	}

	wl_event_source_remove(ipc_event_source);
	close(ipc_socket_fd);
	ipc_socket_fd = -1;

	if (ipc_socket_path) {
		unlink(ipc_socket_path);
		unsetenv("LABWC_IPC_SOCK");
		free(ipc_socket_path);
		ipc_socket_path = NULL;
	}
}

/* ===================================================================
 * Section 2: JSON Builders
 * =================================================================== */

static struct json_object *
ipc_json_position(struct wlr_box box)
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "x", json_object_new_int(box.x));
	json_object_object_add(obj, "y", json_object_new_int(box.y));
	return obj;
}

static struct json_object *
ipc_json_dimension(struct wlr_box box)
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "width", json_object_new_int(box.width));
	json_object_object_add(obj, "height", json_object_new_int(box.height));
	return obj;
}

static struct json_object *
ipc_json_rect(struct wlr_box box)
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "position", ipc_json_position(box));
	json_object_object_add(obj, "dimension", ipc_json_dimension(box));
	return obj;
}

static struct json_object *
ipc_json_output_mode(int width, int height, int refresh)
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "width", json_object_new_int(width));
	json_object_object_add(obj, "height", json_object_new_int(height));
	json_object_object_add(obj, "refresh", json_object_new_int(refresh));
	return obj;
}

static const char *
transform_name(enum wl_output_transform t)
{
	switch (t) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
		return "normal";
	case WL_OUTPUT_TRANSFORM_90:
		return "90";
	case WL_OUTPUT_TRANSFORM_180:
		return "180";
	case WL_OUTPUT_TRANSFORM_270:
		return "270";
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		return "flipped";
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		return "flipped-90";
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		return "flipped-180";
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		return "flipped-270";
	}
	return "normal";
}

static const char *
subpixel_name(enum wl_output_subpixel sp)
{
	switch (sp) {
	case WL_OUTPUT_SUBPIXEL_UNKNOWN:
		return "unknown";
	case WL_OUTPUT_SUBPIXEL_NONE:
		return "none";
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
		return "rgb";
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
		return "bgr";
	case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
		return "vrgb";
	case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
		return "vbgr";
	}
	return "unknown";
}

/* --- Type 1: GET_WORKSPACES --- */
static struct json_object *
ipc_json_get_workspaces(void)
{
	struct json_object *arr = json_object_new_array();
	int index = 0;
	struct workspace *ws;
	wl_list_for_each(ws, &server.workspaces.all, link) {
		index++;
		struct json_object *obj = json_object_new_object();
		json_object_object_add(obj, "num", json_object_new_int(index));
		json_object_object_add(obj, "name",
			json_object_new_string(ws->name));
		json_object_object_add(obj, "visible",
			json_object_new_boolean(ws->tree->node.enabled));
		json_object_object_add(obj, "focused",
			json_object_new_boolean(ws
				== server.workspaces.current));
		json_object_object_add(obj, "urgent",
			json_object_new_boolean(false));

		/* Use the first usable output's area as workspace rect */
		struct wlr_box rect = {0};
		struct output *out;
		wl_list_for_each(out, &server.outputs, link) {
			if (output_is_usable(out)) {
				rect = output_usable_area_in_layout_coords(out);
				json_object_object_add(obj, "output",
					json_object_new_string(
						out->wlr_output->name));
				break;
			}
		}
		json_object_object_add(obj, "rect", ipc_json_rect(rect));
		json_object_array_add(arr, obj);
	}
	return arr;
}

/* --- Type 3: GET_OUTPUTS --- */
static struct json_object *
ipc_json_get_outputs(void)
{
	struct json_object *arr = json_object_new_array();
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		struct wlr_output *o = output->wlr_output;
		struct json_object *obj = json_object_new_object();
		json_object_object_add(obj, "name",
			json_object_new_string(o->name ? o->name : ""));
		json_object_object_add(obj, "make",
			json_object_new_string(o->make ? o->make : "Unknown"));
		json_object_object_add(obj, "model",
			json_object_new_string(o->model ? o->model
							: "Unknown"));
		json_object_object_add(obj, "serial",
			json_object_new_string(o->serial ? o->serial : ""));
		json_object_object_add(obj, "active",
			json_object_new_boolean(o->enabled));
		json_object_object_add(obj, "dpms",
			json_object_new_boolean(o->enabled));
		json_object_object_add(obj, "power",
			json_object_new_boolean(o->enabled));
		json_object_object_add(obj, "primary",
			json_object_new_boolean(false));
		json_object_object_add(obj, "scale",
			json_object_new_double(o->scale));
		json_object_object_add(obj, "subpixel_hinting",
			json_object_new_string(subpixel_name(o->subpixel)));
		json_object_object_add(obj, "transform",
			json_object_new_string(transform_name(o->transform)));

		/* current_workspace */
		const char *ws_name = NULL;
		if (output_is_usable(output) && server.workspaces.current) {
			ws_name = server.workspaces.current->name;
		}
		json_object_object_add(obj, "current_workspace",
			ws_name ? json_object_new_string(ws_name)
				: json_object_new_null());

		/* modes */
		struct json_object *modes = json_object_new_array();
		struct wlr_output_mode *mode;
		wl_list_for_each(mode, &o->modes, link) {
			json_object_array_add(modes,
				ipc_json_output_mode(mode->width, mode->height,
					mode->refresh));
		}
		json_object_object_add(obj, "modes", modes);

		/* current_mode */
		if (o->current_mode) {
			json_object_object_add(obj, "current_mode",
				ipc_json_output_mode(o->current_mode->width,
					o->current_mode->height,
					o->current_mode->refresh));
		} else {
			json_object_object_add(obj, "current_mode",
				ipc_json_output_mode(o->width, o->height,
					o->refresh));
		}

		/* rect */
		struct wlr_box rect = {0};
		if (output_is_usable(output)) {
			rect.x = output->scene_output->x;
			rect.y = output->scene_output->y;
		}
		rect.width = o->width;
		rect.height = o->height;
		json_object_object_add(obj, "rect", ipc_json_rect(rect));

		json_object_array_add(arr, obj);
	}
	return arr;
}

/* --- Type 4: GET_TREE helpers --- */

static const char *
border_name(struct view *view)
{
	if (view->ssd_preference == LAB_SSD_PREF_CLIENT
		&& view->ssd_mode == LAB_SSD_MODE_NONE) {
		return "csd";
	}
	switch (view->ssd_mode) {
	case LAB_SSD_MODE_FULL:
		return "normal";
	case LAB_SSD_MODE_BORDER:
		return "pixel";
	case LAB_SSD_MODE_NONE:
		return "none";
	default:
		break;
	}
	return "none";
}

static struct json_object *
ipc_json_view_node(struct view *view)
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "id",
		json_object_new_int64((int64_t)view->creation_id));
	json_object_object_add(obj, "name",
		json_object_new_string(view->title));
	json_object_object_add(obj, "type",
		json_object_new_string("floating_con"));

	/* app_id / shell / window_properties */
	if (view->type == LAB_XDG_SHELL_VIEW) {
		json_object_object_add(obj, "app_id",
			json_object_new_string(view->app_id));
		json_object_object_add(obj, "identifier",
			json_object_new_string(foreign_toplevel_get_identifier(
				view->foreign_toplevel)));
		json_object_object_add(obj, "shell",
			json_object_new_string("xdg_shell"));
		json_object_object_add(obj, "window", json_object_new_null());
	}
#if HAVE_XWAYLAND
	else if (view->type == LAB_XWAYLAND_VIEW) {
		json_object_object_add(obj, "app_id", json_object_new_null());
		json_object_object_add(obj, "identifier",
			json_object_new_string(foreign_toplevel_get_identifier(
				view->foreign_toplevel)));
		json_object_object_add(obj, "shell",
			json_object_new_string("xwayland"));

		struct wlr_xwayland_surface *xsurface = NULL;
		if (view->surface) {
			xsurface = wlr_xwayland_surface_try_from_wlr_surface(
				view->surface);
		}
		if (xsurface) {
			json_object_object_add(obj, "window",
				json_object_new_int64((int64_t)
						xsurface->window_id));
			struct json_object *wp = json_object_new_object();
			json_object_object_add(wp, "class",
				json_object_new_string(xsurface->class
						? xsurface->class
						: ""));
			json_object_object_add(wp, "instance",
				json_object_new_string(xsurface->instance
						? xsurface->instance
						: ""));
			json_object_object_add(wp, "title",
				json_object_new_string(view->title));
			json_object_object_add(wp, "window_role",
				json_object_new_string(xsurface->role
						? xsurface->role
						: ""));
			json_object_object_add(wp, "window_type",
				json_object_new_string("normal"));
			json_object_object_add(wp, "transient_for",
				json_object_new_null());
			json_object_object_add(obj, "window_properties", wp);
		} else {
			json_object_object_add(obj, "window",
				json_object_new_null());
		}
	}
#endif

	pid_t pid = -1;
	if (view->impl && view->impl->get_pid) {
		pid = view->impl->get_pid(view);
	}
	json_object_object_add(obj, "pid", json_object_new_int(pid));

	json_object_object_add(obj, "visible",
		json_object_new_boolean(view->mapped
			&& view->workspace == server.workspaces.current));
	json_object_object_add(obj, "focused",
		json_object_new_boolean(view == server.active_view));
	json_object_object_add(obj, "fullscreen_mode",
		json_object_new_int(view->fullscreen ? 1 : 0));
	json_object_object_add(obj, "sticky",
		json_object_new_boolean(view->visible_on_all_workspaces));

	json_object_object_add(obj, "border",
		json_object_new_string(border_name(view)));
	json_object_object_add(obj, "current_border_width",
		json_object_new_int(view->ssd_mode != LAB_SSD_MODE_NONE ? 1
									: 0));

	/* geometry */
	json_object_object_add(obj, "position",
		ipc_json_position(view->current));
	json_object_object_add(obj, "dimension",
		ipc_json_dimension(view->current));

	return obj;
}

static struct json_object *
ipc_json_get_tree(void)
{
	struct json_object *root = json_object_new_object();
	json_object_object_add(root, "id", json_object_new_int(1));
	json_object_object_add(root, "name", json_object_new_string("root"));
	json_object_object_add(root, "type", json_object_new_string("root"));

	/* Compute bounding box of all outputs */
	struct wlr_box root_rect = {0};
	struct output *out;
	wl_list_for_each(out, &server.outputs, link) {
		if (!output_is_usable(out)) {
			continue;
		}
		int ox = out->scene_output->x;
		int oy = out->scene_output->y;
		int ow = out->wlr_output->width;
		int oh = out->wlr_output->height;
		if (ox + ow > root_rect.width) {
			root_rect.width = ox + ow;
		}
		if (oy + oh > root_rect.height) {
			root_rect.height = oy + oh;
		}
	}
	json_object_object_add(root, "position", ipc_json_position(root_rect));
	json_object_object_add(root, "dimension",
		ipc_json_dimension(root_rect));

	struct json_object *output_nodes = json_object_new_array();
	int out_idx = 0;
	wl_list_for_each(out, &server.outputs, link) {
		out_idx++;
		struct json_object *out_obj = json_object_new_object();
		json_object_object_add(out_obj, "id",
			json_object_new_int(0x10000000 | out_idx));
		json_object_object_add(out_obj, "name",
			json_object_new_string(out->wlr_output->name
					? out->wlr_output->name
					: ""));
		json_object_object_add(out_obj, "type",
			json_object_new_string("output"));

		struct wlr_box out_rect = {0};
		if (output_is_usable(out)) {
			out_rect.x = out->scene_output->x;
			out_rect.y = out->scene_output->y;
		}
		out_rect.width = out->wlr_output->width;
		out_rect.height = out->wlr_output->height;
		json_object_object_add(out_obj, "position",
			ipc_json_position(out_rect));
		json_object_object_add(out_obj, "dimension",
			ipc_json_dimension(out_rect));
		json_object_object_add(out_obj, "layout",
			json_object_new_string("output"));

		/* Build workspace nodes */
		struct json_object *ws_nodes = json_object_new_array();
		int ws_idx = 0;
		struct workspace *ws;
		wl_list_for_each(ws, &server.workspaces.all, link) {
			ws_idx++;
			struct json_object *ws_obj = json_object_new_object();
			json_object_object_add(ws_obj, "id",
				json_object_new_int(0x20000000 | ws_idx));
			json_object_object_add(ws_obj, "name",
				json_object_new_string(ws->name));
			json_object_object_add(ws_obj, "type",
				json_object_new_string("workspace"));
			json_object_object_add(ws_obj, "position",
				ipc_json_position(out_rect));
			json_object_object_add(ws_obj, "dimension",
				ipc_json_dimension(out_rect));

			/* Floating nodes (views) */
			struct json_object *floating = json_object_new_array();
			struct json_object *focus_order =
				json_object_new_array();
			struct view *view;
			wl_list_for_each(view, &server.views, link) {
				if (!view->mapped) {
					continue;
				}
				if (view->workspace != ws
					&& !view->visible_on_all_workspaces) {
					continue;
				}
				json_object_array_add(floating,
					ipc_json_view_node(view));
				json_object_array_add(focus_order,
					json_object_new_int64((int64_t)
							view->creation_id));
			}
			json_object_object_add(ws_obj, "floating_nodes",
				floating);
			json_object_object_add(ws_obj, "focus", focus_order);
			json_object_array_add(ws_nodes, ws_obj);
		}
		json_object_object_add(out_obj, "nodes", ws_nodes);
		json_object_array_add(output_nodes, out_obj);
	}

	json_object_object_add(root, "nodes", output_nodes);
	json_object_object_add(root, "focus", json_object_new_array());
	return root;
}

/* --- Type 7: GET_VERSION --- */
static struct json_object *
ipc_json_get_version(void)
{
	struct json_object *obj = json_object_new_object();

	/* Parse version string X.Y.Z */
	const char *ver = LABWC_VERSION;
	int major = 0, minor = 0, patch = 0;
	sscanf(ver, "%d.%d.%d", &major, &minor, &patch);

	json_object_object_add(obj, "major", json_object_new_int(major));
	json_object_object_add(obj, "minor", json_object_new_int(minor));
	json_object_object_add(obj, "patch", json_object_new_int(patch));

	char human[128];
	snprintf(human, sizeof(human), "labwc %s", ver);
	json_object_object_add(obj, "human_readable",
		json_object_new_string(human));

	json_object_object_add(obj, "loaded_config_file_name",
		json_object_new_string(rc.loaded_config_file
				? rc.loaded_config_file
				: ""));

	return obj;
}

/* --- Type 9: GET_CONFIG --- */
static struct json_object *
ipc_json_get_config(void)
{
	struct json_object *obj = json_object_new_object();
	struct buf b = BUF_INIT;
	if (rc.loaded_config_file) {
		b = buf_from_file(rc.loaded_config_file);
	}
	json_object_object_add(obj, "config",
		json_object_new_string(b.data ? b.data : ""));
	buf_reset(&b);
	return obj;
}

/* --- Type 100: GET_INPUTS --- */

static const char *
input_type_name(enum wlr_input_device_type type)
{
	switch (type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		return "keyboard";
	case WLR_INPUT_DEVICE_POINTER:
		return "pointer";
	case WLR_INPUT_DEVICE_TOUCH:
		return "touch";
	case WLR_INPUT_DEVICE_TABLET:
		return "tablet_tool";
	case WLR_INPUT_DEVICE_TABLET_PAD:
		return "tablet_pad";
	default:
		return "unknown";
	}
}

static struct json_object *
ipc_json_input_device(struct input *input)
{
	struct wlr_input_device *dev = input->wlr_input_device;
	struct json_object *obj = json_object_new_object();

	/* Get vendor/product from libinput if available */
	unsigned int vendor = 0, product = 0;
	if (wlr_input_device_is_libinput(dev)) {
		struct libinput_device *ldev =
			wlr_libinput_get_device_handle(dev);
		if (ldev) {
			vendor = libinput_device_get_id_vendor(ldev);
			product = libinput_device_get_id_product(ldev);
		}
	}

	/* identifier: vendor:product:name */
	char ident[256];
	snprintf(ident, sizeof(ident), "%u:%u:%s", vendor, product,
		dev->name ? dev->name : "");
	/* Replace spaces with underscores in identifier */
	for (char *p = ident; *p; p++) {
		if (*p == ' ') {
			*p = '_';
		}
	}
	json_object_object_add(obj, "identifier",
		json_object_new_string(ident));
	json_object_object_add(obj, "name",
		json_object_new_string(dev->name ? dev->name : ""));
	json_object_object_add(obj, "vendor", json_object_new_int(vendor));
	json_object_object_add(obj, "product", json_object_new_int(product));
	json_object_object_add(obj, "type",
		json_object_new_string(input_type_name(dev->type)));

	/* Keyboard-specific: xkb layout info */
	if (dev->type == WLR_INPUT_DEVICE_KEYBOARD) {
		struct wlr_keyboard *kb = server.seat.keyboard_group
			? &server.seat.keyboard_group->keyboard
			: NULL;
		if (kb && kb->keymap) {
			struct xkb_keymap *keymap = kb->keymap;
			xkb_layout_index_t num_layouts =
				xkb_keymap_num_layouts(keymap);
			xkb_layout_index_t active =
				kb->modifiers.group < num_layouts
				? kb->modifiers.group
				: 0;

			const char *active_name =
				xkb_keymap_layout_get_name(keymap, active);
			json_object_object_add(obj, "xkb_active_layout_name",
				json_object_new_string(active_name ? active_name
								   : ""));
			json_object_object_add(obj, "xkb_active_layout_index",
				json_object_new_int(active));

			struct json_object *layouts = json_object_new_array();
			for (xkb_layout_index_t i = 0; i < num_layouts; i++) {
				const char *name =
					xkb_keymap_layout_get_name(keymap, i);
				json_object_array_add(layouts,
					json_object_new_string(name ? name
								    : ""));
			}
			json_object_object_add(obj, "xkb_layout_names",
				layouts);
		}
	}

	/* Pointer-specific: scroll_factor */
	if (dev->type == WLR_INPUT_DEVICE_POINTER) {
		json_object_object_add(obj, "scroll_factor",
			json_object_new_double(input->scroll_factor));
	}

	return obj;
}

static struct json_object *
ipc_json_get_inputs(void)
{
	struct json_object *arr = json_object_new_array();
	struct input *input;
	wl_list_for_each(input, &server.seat.inputs, link) {
		json_object_array_add(arr, ipc_json_input_device(input));
	}
	return arr;
}

/* --- Type 101: GET_SEATS --- */
static struct json_object *
ipc_json_get_seats(void)
{
	struct json_object *arr = json_object_new_array();
	struct json_object *seat_obj = json_object_new_object();

	json_object_object_add(seat_obj, "name",
		json_object_new_string(server.seat.wlr_seat->name
				? server.seat.wlr_seat->name
				: "seat0"));
	json_object_object_add(seat_obj, "capabilities",
		json_object_new_int(server.seat.wlr_seat->capabilities));
	json_object_object_add(seat_obj, "focus",
		json_object_new_int64(server.active_view
				? (int64_t)server.active_view->creation_id
				: 0));

	struct json_object *devices = json_object_new_array();
	struct input *input;
	wl_list_for_each(input, &server.seat.inputs, link) {
		json_object_array_add(devices, ipc_json_input_device(input));
	}
	json_object_object_add(seat_obj, "devices", devices);
	json_object_array_add(arr, seat_obj);
	return arr;
}

/* ===================================================================
 * Section 3: Sway-Compatible Command Parser
 * =================================================================== */

/*
 * Helper to create an action, add a single string arg, execute it via
 * the action system, then free it.
 */
static bool
run_action_simple(const char *action_name, struct view *target)
{
	struct action *a = action_create(action_name);
	if (!a) {
		return false;
	}
	struct wl_list actions;
	wl_list_init(&actions);
	wl_list_insert(&actions, &a->link);
	actions_run(target, &actions, NULL);
	action_list_free(&actions);
	return true;
}

static bool
run_action_with_str(const char *action_name, const char *key, const char *value,
	struct view *target)
{
	struct action *a = action_create(action_name);
	if (!a) {
		return false;
	}
	action_arg_add_str(a, key, value);
	struct wl_list actions;
	wl_list_init(&actions);
	wl_list_insert(&actions, &a->link);
	actions_run(target, &actions, NULL);
	action_list_free(&actions);
	return true;
}

/*
 * Parse criteria like [app_id="firefox" title="..."]
 * Returns the start of the command after criteria, or cmd if no criteria.
 * Sets *query to a newly allocated query if criteria found, else NULL.
 */
static const char *
parse_criteria(const char *cmd, struct view_query **query)
{
	*query = NULL;
	while (*cmd == ' ' || *cmd == '\t') {
		cmd++;
	}
	if (*cmd != '[') {
		return cmd;
	}

	const char *p = cmd + 1;
	*query = view_query_create();

	while (*p && *p != ']') {
		while (*p == ' ') {
			p++;
		}
		if (*p == ']') {
			break;
		}

		/* Parse key=value or key="value" */
		const char *key_start = p;
		while (*p && *p != '=' && *p != ']' && *p != ' ') {
			p++;
		}
		size_t key_len = p - key_start;
		if (*p != '=') {
			break;
		}
		p++; /* skip '=' */

		/* Parse value (optionally quoted) */
		const char *val_start;
		const char *val_end;
		if (*p == '"') {
			p++;
			val_start = p;
			while (*p && *p != '"') {
				p++;
			}
			val_end = p;
			if (*p == '"') {
				p++;
			}
		} else {
			val_start = p;
			while (*p && *p != ' ' && *p != ']') {
				p++;
			}
			val_end = p;
		}

		char *val = strndup(val_start, val_end - val_start);
		if (key_len == 6 && !strncmp(key_start, "app_id", 6)) {
			free((*query)->identifier);
			(*query)->identifier = val;
		} else if (key_len == 5 && !strncmp(key_start, "title", 5)) {
			free((*query)->title);
			(*query)->title = val;
		} else if (key_len == 5 && !strncmp(key_start, "class", 5)) {
			free((*query)->identifier);
			(*query)->identifier = val;
		} else {
			free(val);
		}
	}

	if (*p == ']') {
		p++;
	}
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	return p;
}

/* Find the first matching view for a query */
static struct view *
find_view_for_query(struct view_query *query)
{
	struct view *view;
	wl_list_for_each(view, &server.views, link) {
		if (view->mapped && view_matches_query(view, query)) {
			return view;
		}
	}
	return NULL;
}

static struct json_object *
cmd_result(bool success, const char *error)
{
	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "success",
		json_object_new_boolean(success));
	if (error) {
		json_object_object_add(obj, "error",
			json_object_new_string(error));
	}
	return obj;
}

static struct json_object *
execute_single_command(const char *cmd)
{
	/* Parse criteria */
	struct view_query *query = NULL;
	cmd = parse_criteria(cmd, &query);

	/* Determine target view */
	struct view *target = NULL;
	if (query) {
		target = find_view_for_query(query);
		view_query_free(query);
	} else {
		target = server.active_view;
	}

	/* Skip leading whitespace */
	while (*cmd == ' ' || *cmd == '\t') {
		cmd++;
	}

	/* --- nop --- */
	if (!strncmp(cmd, "nop", 3)) {
		return cmd_result(true, NULL);
	}

	/* --- exit --- */
	if (!strcmp(cmd, "exit")) {
		run_action_simple("Exit", NULL);
		return cmd_result(true, NULL);
	}

	/* --- reload --- */
	if (!strcmp(cmd, "reload")) {
		kill(getpid(), SIGHUP);
		return cmd_result(true, NULL);
	}

	/* --- exec --- */
	if (!strncmp(cmd, "exec ", 5)) {
		const char *exec_cmd = cmd + 5;
		while (*exec_cmd == ' ') {
			exec_cmd++;
		}
		/* skip optional --no-startup-id */
		if (!strncmp(exec_cmd, "--no-startup-id ", 16)) {
			exec_cmd += 16;
			while (*exec_cmd == ' ') {
				exec_cmd++;
			}
		}
		if (*exec_cmd) {
			run_action_with_str("Execute", "command", exec_cmd,
				NULL);
			return cmd_result(true, NULL);
		}
		return cmd_result(false, "exec requires a command");
	}

	/* --- kill --- */
	if (!strcmp(cmd, "kill")) {
		if (!target) {
			return cmd_result(false, "No focused window");
		}
		run_action_simple("Close", target);
		return cmd_result(true, NULL);
	}

	/* --- fullscreen toggle --- */
	if (!strcmp(cmd, "fullscreen toggle") || !strcmp(cmd, "fullscreen")) {
		if (!target) {
			return cmd_result(false, "No focused window");
		}
		run_action_simple("ToggleFullscreen", target);
		return cmd_result(true, NULL);
	}

	/* --- floating toggle --- */
	if (!strcmp(cmd, "floating toggle")) {
		if (!target) {
			return cmd_result(false, "No focused window");
		}
		/* In stacking WM: untile if tiled, otherwise no-op */
		if (view_is_tiled(target)) {
			run_action_simple("UnSnap", target);
		}
		return cmd_result(true, NULL);
	}

	/* --- sticky toggle --- */
	if (!strcmp(cmd, "sticky toggle")) {
		if (!target) {
			return cmd_result(false, "No focused window");
		}
		run_action_simple("ToggleOmnipresent", target);
		return cmd_result(true, NULL);
	}

	/* --- border --- */
	if (!strncmp(cmd, "border ", 7)) {
		if (!target) {
			return cmd_result(false, "No focused window");
		}
		const char *arg = cmd + 7;
		if (!strcmp(arg, "none")) {
			run_action_with_str("SetDecorations", "decorations",
				"none", target);
		} else if (!strcmp(arg, "normal")) {
			run_action_with_str("SetDecorations", "decorations",
				"full", target);
		} else if (!strcmp(arg, "pixel")) {
			run_action_with_str("SetDecorations", "decorations",
				"border", target);
		} else if (!strcmp(arg, "toggle")) {
			run_action_simple("ToggleDecorations", target);
		} else {
			return cmd_result(false, "Unknown border type");
		}
		return cmd_result(true, NULL);
	}

	/* --- workspace --- */
	if (!strncmp(cmd, "workspace ", 10)) {
		const char *ws_name = cmd + 10;
		while (*ws_name == ' ') {
			ws_name++;
		}
		const char *to = ws_name;
		if (!strcmp(ws_name, "next")
			|| !strcmp(ws_name, "next_on_output")) {
			to = "right";
		} else if (!strcmp(ws_name, "prev")
			|| !strcmp(ws_name, "prev_on_output")) {
			to = "left";
		}
		run_action_with_str("GoToDesktop", "to", to, NULL);
		return cmd_result(true, NULL);
	}

	/* --- focus output --- */
	if (!strncmp(cmd, "focus output ", 13)) {
		const char *arg = cmd + 13;
		while (*arg == ' ') {
			arg++;
		}
		/* Try as direction first */
		if (!strcmp(arg, "left") || !strcmp(arg, "right")
			|| !strcmp(arg, "up") || !strcmp(arg, "down")) {
			run_action_with_str("FocusOutput", "direction", arg,
				NULL);
		} else {
			run_action_with_str("FocusOutput", "output", arg, NULL);
		}
		return cmd_result(true, NULL);
	}

	/* --- move ... --- */
	if (!strncmp(cmd, "move ", 5)) {
		const char *arg = cmd + 5;
		while (*arg == ' ') {
			arg++;
		}

		/* move position X Y */
		if (!strncmp(arg, "position ", 9)) {
			if (!target) {
				return cmd_result(false, "No focused window");
			}
			int x = 0, y = 0;
			sscanf(arg + 9, "%d %d", &x, &y);
			struct action *a = action_create("MoveTo");
			if (a) {
				action_arg_add_int(a, "x", x);
				action_arg_add_int(a, "y", y);
				struct wl_list actions;
				wl_list_init(&actions);
				wl_list_insert(&actions, &a->link);
				actions_run(target, &actions, NULL);
				action_list_free(&actions);
			}
			return cmd_result(true, NULL);
		}

		/* move [container|window] [to] workspace <name> */
		const char *ws_needle = strstr(arg, "workspace ");
		if (ws_needle) {
			if (!target) {
				return cmd_result(false, "No focused window");
			}
			const char *ws_name = ws_needle + 10;
			while (*ws_name == ' ') {
				ws_name++;
			}
			run_action_with_str("SendToDesktop", "to", ws_name,
				target);
			return cmd_result(true, NULL);
		}

		/* move [container|window] [to] output <name|dir> */
		const char *out_needle = strstr(arg, "output ");
		if (out_needle) {
			if (!target) {
				return cmd_result(false, "No focused window");
			}
			const char *out_name = out_needle + 7;
			while (*out_name == ' ') {
				out_name++;
			}
			if (!strcmp(out_name, "left")
				|| !strcmp(out_name, "right")
				|| !strcmp(out_name, "up")
				|| !strcmp(out_name, "down")) {
				run_action_with_str("MoveToOutput", "direction",
					out_name, target);
			} else {
				run_action_with_str("MoveToOutput", "output",
					out_name, target);
			}
			return cmd_result(true, NULL);
		}

		/* move left|right|up|down [N] */
		if (!strncmp(arg, "left", 4) || !strncmp(arg, "right", 5)
			|| !strncmp(arg, "up", 2) || !strncmp(arg, "down", 4)) {
			if (!target) {
				return cmd_result(false, "No focused window");
			}
			int amount = 10;
			const char *num = arg;
			/* Skip past direction word */
			while (*num && *num != ' ') {
				num++;
			}
			while (*num == ' ') {
				num++;
			}
			if (*num >= '0' && *num <= '9') {
				amount = atoi(num);
			}

			struct action *a = action_create("MoveRelative");
			if (a) {
				int dx = 0, dy = 0;
				if (!strncmp(arg, "left", 4)) {
					dx = -amount;
				} else if (!strncmp(arg, "right", 5)) {
					dx = amount;
				} else if (!strncmp(arg, "up", 2)) {
					dy = -amount;
				} else if (!strncmp(arg, "down", 4)) {
					dy = amount;
				}
				action_arg_add_int(a, "x", dx);
				action_arg_add_int(a, "y", dy);
				struct wl_list actions;
				wl_list_init(&actions);
				wl_list_insert(&actions, &a->link);
				actions_run(target, &actions, NULL);
				action_list_free(&actions);
			}
			return cmd_result(true, NULL);
		}

		return cmd_result(false, "Unknown move command");
	}

	/* --- resize --- */
	if (!strncmp(cmd, "resize ", 7)) {
		if (!target) {
			return cmd_result(false, "No focused window");
		}
		const char *arg = cmd + 7;

		/* resize set W H */
		if (!strncmp(arg, "set ", 4)) {
			int w = 0, h = 0;
			sscanf(arg + 4, "%d %d", &w, &h);
			struct action *a = action_create("ResizeTo");
			if (a) {
				action_arg_add_int(a, "width", w);
				action_arg_add_int(a, "height", h);
				struct wl_list actions;
				wl_list_init(&actions);
				wl_list_insert(&actions, &a->link);
				actions_run(target, &actions, NULL);
				action_list_free(&actions);
			}
			return cmd_result(true, NULL);
		}

		/* resize grow|shrink width|height N [px|ppt] */
		if (!strncmp(arg, "grow ", 5) || !strncmp(arg, "shrink ", 7)) {
			bool grow = !strncmp(arg, "grow", 4);
			const char *rest = arg + (grow ? 5 : 7);
			bool is_width = !strncmp(rest, "width", 5);
			/* Skip past width/height */
			while (*rest && *rest != ' ') {
				rest++;
			}
			while (*rest == ' ') {
				rest++;
			}
			int amount = 10;
			if (*rest >= '0' && *rest <= '9') {
				amount = atoi(rest);
			}
			if (!grow) {
				amount = -amount;
			}

			struct action *a = action_create("ResizeRelative");
			if (a) {
				if (is_width) {
					action_arg_add_int(a, "right", amount);
				} else {
					action_arg_add_int(a, "bottom", amount);
				}
				struct wl_list actions;
				wl_list_init(&actions);
				wl_list_insert(&actions, &a->link);
				actions_run(target, &actions, NULL);
				action_list_free(&actions);
			}
			return cmd_result(true, NULL);
		}

		return cmd_result(false, "Unknown resize command");
	}

	/* --- Unsupported commands --- */
	if (!strncmp(cmd, "layout ", 7) || !strncmp(cmd, "split", 5)) {
		return cmd_result(false,
			"layout/split not supported (stacking compositor)");
	}
	if (!strncmp(cmd, "mark ", 5) || !strncmp(cmd, "unmark", 6)) {
		return cmd_result(false, "marks not supported");
	}
	if (!strncmp(cmd, "scratchpad", 10)) {
		return cmd_result(false, "scratchpad not supported");
	}

	return cmd_result(false, "Unknown command");
}

/* Parse and run possibly multi-command string (separated by ; or ,) */
static struct json_object *
ipc_cmd_run(const char *payload)
{
	struct json_object *results = json_object_new_array();
	if (!payload || !*payload) {
		json_object_array_add(results,
			cmd_result(false, "No command given"));
		return results;
	}

	/* Split on ; */
	char *buf = strdup(payload);
	char *saveptr = NULL;
	char *token = strtok_r(buf, ";", &saveptr);
	while (token) {
		/* Trim whitespace */
		while (*token == ' ' || *token == '\t') {
			token++;
		}
		char *end = token + strlen(token) - 1;
		while (end > token && (*end == ' ' || *end == '\t')) {
			*end-- = '\0';
		}
		if (*token) {
			json_object_array_add(results,
				execute_single_command(token));
		}
		token = strtok_r(NULL, ";", &saveptr);
	}
	free(buf);

	if (json_object_array_length(results) == 0) {
		json_object_array_add(results,
			cmd_result(false, "No command given"));
	}
	return results;
}

/* ===================================================================
 * Section 3b: SUBSCRIBE handler
 * =================================================================== */

static struct json_object *
ipc_cmd_subscribe(struct ipc_client *client, const char *payload)
{
	struct json_object *parsed = json_tokener_parse(payload);
	if (!parsed || json_object_get_type(parsed) != json_type_array) {
		if (parsed) {
			json_object_put(parsed);
		}
		return cmd_result(false, "Invalid subscription payload");
	}

	int len = json_object_array_length(parsed);
	for (int i = 0; i < len; i++) {
		struct json_object *item = json_object_array_get_idx(parsed, i);
		const char *name = json_object_get_string(item);
		if (!name) {
			continue;
		}
		if (!strcmp(name, "workspace")) {
			client->subscriptions |= IPC_SUB_WORKSPACE;
		} else if (!strcmp(name, "output")) {
			client->subscriptions |= IPC_SUB_OUTPUT;
		} else if (!strcmp(name, "window")) {
			client->subscriptions |= IPC_SUB_WINDOW;
		} else if (!strcmp(name, "shutdown")) {
			client->subscriptions |= IPC_SUB_SHUTDOWN;
		} else if (!strcmp(name, "tick")) {
			client->subscriptions |= IPC_SUB_TICK;
			/* Send initial tick */
			struct json_object *tick = json_object_new_object();
			json_object_object_add(tick, "first",
				json_object_new_boolean(true));
			json_object_object_add(tick, "payload",
				json_object_new_string(""));
			ipc_send_reply_json(client, IPC_EVENT_TICK, tick);
		}
	}
	json_object_put(parsed);
	return cmd_result(true, NULL);
}

/* ===================================================================
 * Message dispatch
 * =================================================================== */

static void
ipc_handle_message(struct ipc_client *client, uint32_t type,
	const char *payload, uint32_t len)
{
	struct json_object *reply = NULL;

	switch (type) {
	case IPC_RUN_COMMAND:
		reply = ipc_cmd_run(payload);
		ipc_send_reply_json(client, IPC_RUN_COMMAND, reply);
		return;

	case IPC_GET_WORKSPACES:
		reply = ipc_json_get_workspaces();
		break;

	case IPC_SUBSCRIBE:
		reply = ipc_cmd_subscribe(client, payload);
		break;

	case IPC_GET_OUTPUTS:
		reply = ipc_json_get_outputs();
		break;

	case IPC_GET_TREE:
		reply = ipc_json_get_tree();
		break;

	case IPC_GET_BAR_CONFIG:
		/* labwc has no built-in bar */
		if (payload && *payload) {
			/* Requesting specific bar config - return empty object
			 */
			reply = json_object_new_object();
		} else {
			reply = json_object_new_array();
		}
		break;

	case IPC_GET_VERSION:
		reply = ipc_json_get_version();
		break;

	case IPC_GET_CONFIG:
		reply = ipc_json_get_config();
		break;

	case IPC_SEND_TICK: {
		/* Broadcast tick event to subscribers */
		struct json_object *tick_event = json_object_new_object();
		json_object_object_add(tick_event, "first",
			json_object_new_boolean(false));
		json_object_object_add(tick_event, "payload",
			json_object_new_string(payload ? payload : ""));
		const char *tick_str = json_object_to_json_string(tick_event);
		struct ipc_client *c;
		wl_list_for_each(c, &ipc_clients, link) {
			if (c->subscriptions & IPC_SUB_TICK) {
				ipc_send_reply(c, IPC_EVENT_TICK, tick_str,
					strlen(tick_str));
			}
		}
		json_object_put(tick_event);
		reply = cmd_result(true, NULL);
		break;
	}

	case IPC_SYNC:
		/* Not applicable to Wayland (same as sway) */
		reply = cmd_result(false, NULL);
		break;

	case IPC_GET_INPUTS:
		reply = ipc_json_get_inputs();
		break;

	case IPC_GET_SEATS:
		reply = ipc_json_get_seats();
		break;

	default:
		reply = cmd_result(false, "Unknown IPC message type");
		break;
	}

	ipc_send_reply_json(client, type, reply);
}

/* ===================================================================
 * Section 4: Event Emitters
 * =================================================================== */

static void
ipc_broadcast_event(uint32_t event_type, uint32_t sub_bit,
	struct json_object *event)
{
	if (ipc_socket_fd < 0) {
		json_object_put(event);
		return;
	}

	const char *str = json_object_to_json_string(event);
	size_t len = strlen(str);
	struct ipc_client *client;
	wl_list_for_each(client, &ipc_clients, link) {
		if (client->subscriptions & sub_bit) {
			ipc_send_reply(client, event_type, str, len);
		}
	}
	json_object_put(event);
}

static struct json_object *
ipc_json_workspace_obj(struct workspace *ws)
{
	if (!ws) {
		return json_object_new_null();
	}
	struct json_object *obj = json_object_new_object();
	int index = 0;
	struct workspace *w;
	wl_list_for_each(w, &server.workspaces.all, link) {
		index++;
		if (w == ws) {
			break;
		}
	}
	json_object_object_add(obj, "num", json_object_new_int(index));
	json_object_object_add(obj, "name", json_object_new_string(ws->name));
	json_object_object_add(obj, "visible",
		json_object_new_boolean(ws->tree->node.enabled));
	json_object_object_add(obj, "focused",
		json_object_new_boolean(ws == server.workspaces.current));
	json_object_object_add(obj, "urgent", json_object_new_boolean(false));

	struct wlr_box rect = {0};
	struct output *out;
	wl_list_for_each(out, &server.outputs, link) {
		if (output_is_usable(out)) {
			rect = output_usable_area_in_layout_coords(out);
			json_object_object_add(obj, "output",
				json_object_new_string(out->wlr_output->name));
			break;
		}
	}
	json_object_object_add(obj, "rect", ipc_json_rect(rect));
	return obj;
}

void
ipc_event_workspace(const char *change, struct workspace *current,
	struct workspace *old)
{
	if (ipc_socket_fd < 0) {
		return;
	}
	struct json_object *event = json_object_new_object();
	json_object_object_add(event, "change", json_object_new_string(change));
	json_object_object_add(event, "current",
		ipc_json_workspace_obj(current));
	json_object_object_add(event, "old", ipc_json_workspace_obj(old));
	ipc_broadcast_event(IPC_EVENT_WORKSPACE, IPC_SUB_WORKSPACE, event);
}

void
ipc_event_output(const char *change)
{
	if (ipc_socket_fd < 0) {
		return;
	}
	struct json_object *event = json_object_new_object();
	json_object_object_add(event, "change", json_object_new_string(change));
	ipc_broadcast_event(IPC_EVENT_OUTPUT, IPC_SUB_OUTPUT, event);
}

void
ipc_event_window(const char *change, struct view *view)
{
	if (ipc_socket_fd < 0) {
		return;
	}
	struct json_object *event = json_object_new_object();
	json_object_object_add(event, "change", json_object_new_string(change));
	if (view) {
		json_object_object_add(event, "container",
			ipc_json_view_node(view));
	} else {
		json_object_object_add(event, "container",
			json_object_new_null());
	}
	ipc_broadcast_event(IPC_EVENT_WINDOW, IPC_SUB_WINDOW, event);
}

void
ipc_event_shutdown(void)
{
	if (ipc_socket_fd < 0) {
		return;
	}
	struct json_object *event = json_object_new_object();
	json_object_object_add(event, "change", json_object_new_string("exit"));
	ipc_broadcast_event(IPC_EVENT_SHUTDOWN, IPC_SUB_SHUTDOWN, event);
}
