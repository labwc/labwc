#ifndef __LABWC_H
#define __LABWC_H

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>
#include <xkbcommon/xkbcommon.h>

#include "common/log.h"
#include "config/keybind.h"
#include "config/rcxml.h"

#define XCURSOR_DEFAULT "left_ptr"
#define XCURSOR_SIZE 24
#define XCURSOR_MOVE "grabbing"

enum cursor_mode {
	LAB_CURSOR_PASSTHROUGH,
	LAB_CURSOR_MOVE,
	LAB_CURSOR_RESIZE,
};

struct input {
	struct wlr_input_device *wlr_input_device;
	struct seat *seat;
	struct wl_listener destroy;
	struct wl_list link; /* seat::inputs */
};

struct seat {
	struct wlr_seat *seat;
	struct server *server;
	struct wlr_keyboard_group *keyboard_group;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;

	struct wl_list inputs;
	struct wl_listener new_input;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;
};

struct server {
	struct wl_display *wl_display;
	struct wlr_renderer *renderer;
	struct wlr_backend *backend;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_layer_shell_v1 *layer_shell;

	struct wl_listener new_xdg_surface;
	struct wl_listener new_layer_surface;

	struct wl_listener xdg_toplevel_decoration;
	struct wlr_xwayland *xwayland;
	struct wl_listener new_xwayland_surface;

	struct wl_list views;
	struct wl_list unmanaged_surfaces;

	struct seat seat;

	/* cursor interactive */
	enum cursor_mode cursor_mode;
	struct view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_box;
	uint32_t resize_edges;

	struct wl_list outputs;
	struct wl_listener new_output;
	struct wlr_output_layout *output_layout;

	/* Set when in cycle (alt-tab) mode */
	struct view *cycle_view;
};

struct output {
	struct wl_list link;
	struct server *server;
	struct wlr_output *wlr_output;
	struct wl_list layers[4];
	struct wl_listener frame;
	struct wl_listener destroy;
};

enum view_type { LAB_XDG_SHELL_VIEW, LAB_XWAYLAND_VIEW };

enum deco_part {
	LAB_DECO_NONE = 0,
	LAB_DECO_BUTTON_CLOSE,
	LAB_DECO_BUTTON_MAXIMIZE,
	LAB_DECO_BUTTON_ICONIFY,
	LAB_DECO_PART_TITLE,
	LAB_DECO_PART_TOP,
	LAB_DECO_PART_RIGHT,
	LAB_DECO_PART_BOTTOM,
	LAB_DECO_PART_LEFT,
	LAB_DECO_END_MARKER
};

struct view_impl {
	void (*configure)(struct view *view, struct wlr_box geo);
	void (*close)(struct view *view);
	void (*for_each_surface)(struct view *view,
		wlr_surface_iterator_func_t iterator, void *data);
	void (*map)(struct view *view);
	void (*unmap)(struct view *view);
};

struct border {
	int top;
	int right;
	int bottom;
	int left;
};

struct view {
	struct server *server;
	enum view_type type;
	const struct view_impl *impl;
	struct wl_list link;

	union {
		struct wlr_xdg_surface *xdg_surface;
		struct wlr_xwayland_surface *xwayland_surface;
	};
	struct wlr_surface *surface;

	bool mapped;
	bool been_mapped;
	bool minimized;

	/* geometry of the wlr_surface contained within the view */
	int x, y, w, h;

	/*
	 * margin refers to the space between the extremities of the view and
	 * wlr_surface - typically made up of decoration.
	 * For xdg-shell views, the margin is typically negative.
	 */
	struct border margin;

	int xdg_grab_offset;

	bool server_side_deco;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_configure;
};

struct xwayland_unmanaged {
	struct server *server;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wl_list link;
	int lx, ly;

	struct wl_listener request_configure;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
};

void xdg_toplevel_decoration(struct wl_listener *listener, void *data);
void xdg_surface_new(struct wl_listener *listener, void *data);

void xwayland_surface_new(struct wl_listener *listener, void *data);
void xwayland_unmanaged_create(struct server *server,
	struct wlr_xwayland_surface *xsurface);

/**
 * view_get_surface_geometry - geometry relative to view
 * @view: toplevel containing the surface to process
 * Note: XDG views sometimes have an invisible border, so x and y can be
 * greater than zero.
 */
struct wlr_box view_get_surface_geometry(struct view *view);
struct wlr_box view_geometry(struct view *view);
void view_resize(struct view *view, struct wlr_box geo);
void view_minimize(struct view *view);
void view_unminimize(struct view *view);
void view_for_each_surface(struct view *view,
	wlr_surface_iterator_func_t iterator, void *user_data);

void desktop_focus_view(struct view *view);

/**
 * desktop_next_view - return next view
 * @current: view used as reference point for defining 'next'
 * Note: If current==NULL, the list's second view is returned
 */
struct view *desktop_next_view(struct server *server, struct view *current);
void desktop_focus_next_mapped_view(struct view *current);
struct view *desktop_view_at(struct server *server, double lx, double ly,
			     struct wlr_surface **surface, double *sx,
			     double *sy, int *view_area);

void cursor_init(struct seat *seat);

void keyboard_init(struct seat *seat);

void seat_init(struct server *server);
void seat_finish(struct server *server);
void seat_focus_surface(struct wlr_seat *seat, struct wlr_surface *surface);
struct wlr_surface *seat_focused_surface(void);

void interactive_begin(struct view *view, enum cursor_mode mode,
		       uint32_t edges);

void output_init(struct server *server);

void server_init(struct server *server);
void server_start(struct server *server);
void server_finish(struct server *server);

struct border deco_thickness(struct view *view);
struct wlr_box deco_max_extents(struct view *view);
struct wlr_box deco_box(struct view *view, enum deco_part deco_part);
enum deco_part deco_at(struct view *view, double lx, double ly);

void action(struct server *server, const char *action, const char *command);

void dbg_show_one_view(struct view *view);
void dbg_show_views(struct server *server);
void dbg_show_keybinds();

#endif /* __LABWC_H */
