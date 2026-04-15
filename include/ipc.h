/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IPC_H
#define LABWC_IPC_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>

struct view;
struct workspace;

/* Wire protocol */
#define IPC_MAGIC "labwc-ipc"
#define IPC_MAGIC_LEN 9
#define IPC_HEADER_SIZE (IPC_MAGIC_LEN + 4 + 4) /* 17 bytes */

/* Message types (same numbering as sway/i3) */
enum ipc_msg_type {
	IPC_RUN_COMMAND = 0,
	IPC_GET_WORKSPACES = 1,
	IPC_SUBSCRIBE = 2,
	IPC_GET_OUTPUTS = 3,
	IPC_GET_TREE = 4,
	IPC_GET_MARKS = 5,
	IPC_GET_BAR_CONFIG = 6,
	IPC_GET_VERSION = 7,
	IPC_GET_BINDING_MODES = 8,
	IPC_GET_CONFIG = 9,
	IPC_SEND_TICK = 10,
	IPC_SYNC = 11,
	IPC_GET_BINDING_STATE = 12,
	IPC_GET_INPUTS = 100,
	IPC_GET_SEATS = 101,
};

/* Event types (bit 31 set) */
#define IPC_EVENT_FLAG 0x80000000
enum ipc_event_type {
	IPC_EVENT_WORKSPACE = (IPC_EVENT_FLAG | 0),
	IPC_EVENT_OUTPUT = (IPC_EVENT_FLAG | 1),
	IPC_EVENT_WINDOW = (IPC_EVENT_FLAG | 3),
	IPC_EVENT_SHUTDOWN = (IPC_EVENT_FLAG | 6),
	IPC_EVENT_TICK = (IPC_EVENT_FLAG | 7),
};

/* Subscription bitmask */
#define IPC_SUB_WORKSPACE (1 << 0)
#define IPC_SUB_OUTPUT (1 << 1)
#define IPC_SUB_WINDOW (1 << 3)
#define IPC_SUB_SHUTDOWN (1 << 6)
#define IPC_SUB_TICK (1 << 7)

struct ipc_client {
	struct wl_list link; /* server.ipc_clients */
	int fd;
	struct wl_event_source *readable;
	struct wl_event_source *writable;
	uint32_t subscriptions;
	/* Write buffer for partial writes */
	char *write_buf;
	size_t write_buf_len;
	size_t write_buf_cap;
};

/* Server lifecycle */
void ipc_init(void);
void ipc_finish(void);

/* Event emitters (called from compositor hooks) */
void ipc_event_workspace(const char *change, struct workspace *current,
	struct workspace *old);
void ipc_event_output(const char *change);
void ipc_event_window(const char *change, struct view *view);
void ipc_event_window_geometry(struct view *view, struct wlr_box *new_geo);
void ipc_event_shutdown(void);

#endif /* LABWC_IPC_H */
