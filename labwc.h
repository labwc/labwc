#ifndef LABWC_H
#define LABWC_H

#define _POSIX_C_SOURCE 200112L
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>
#include <xkbcommon/xkbcommon.h>

#define XCURSOR_DEFAULT "left_ptr"
#define XCURSOR_SIZE 24
#define XCURSOR_MOVE "grabbing"
#define XWL_TITLEBAR_HEIGHT (10)
#define XWL_WINDOW_BORDER (3)

/* For brevity's sake, struct members are annotated where they are used. */
enum tinywl_cursor_mode {
	TINYWL_CURSOR_PASSTHROUGH,
	TINYWL_CURSOR_MOVE,
	TINYWL_CURSOR_RESIZE,
};

struct tinywl_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_compositor *compositor;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wlr_xwayland *xwayland;
	struct wl_listener new_xwayland_surface;
	struct wl_list views;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_list keyboards;
	enum tinywl_cursor_mode cursor_mode;
	struct tinywl_view *grabbed_view;
	double grab_x, grab_y;
	int grab_width, grab_height;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;
};

struct tinywl_output {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
};

enum view_type {
	LAB_XDG_SHELL_VIEW,
	LAB_XWAYLAND_VIEW
};

struct tinywl_view {
	enum view_type type;
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wlr_surface *surface;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_configure;

	bool mapped;
	/*
	 * Some X11 windows appear to create additional top levels windows
	 * which we want to ignore. These are never mapped, so we can track
	 * them that way
	 */
	bool been_mapped;
	int x, y;
};

struct tinywl_keyboard {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_input_device *device;

	struct wl_listener modifiers;
	struct wl_listener key;
};

void xdg_surface_map(struct wl_listener *listener, void *data);
void xdg_surface_unmap(struct wl_listener *listener, void *data);
void xdg_surface_destroy(struct wl_listener *listener, void *data);
void xdg_toplevel_request_move(struct wl_listener *listener, void *data);
void xdg_toplevel_request_resize(struct wl_listener *listener, void *data);
void xdg_surface_new(struct wl_listener *listener, void *data);

int xwl_nr_parents(struct tinywl_view *view);
void xwl_surface_map(struct wl_listener *listener, void *data);
void xwl_surface_unmap(struct wl_listener *listener, void *data);
void xwl_surface_destroy(struct wl_listener *listener, void *data);
void xwl_surface_configure(struct wl_listener *listener, void *data);
void xwl_surface_new(struct wl_listener *listener, void *data);

void focus_view(struct tinywl_view *view, struct wlr_surface *surface);
void view_focus_next_toplevel(struct tinywl_server *server);
void begin_interactive(struct tinywl_view *view, enum tinywl_cursor_mode mode, uint32_t edges);
bool is_toplevel(struct tinywl_view *view);
struct tinywl_view *desktop_view_at(struct tinywl_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);

/* TODO: try to refactor to remove from header file */
struct tinywl_view *first_toplevel(struct tinywl_server *server);

void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);
void server_new_output(struct wl_listener *listener, void *data);

void output_frame(struct wl_listener *listener, void *data);

#endif /* LABWC_H */
