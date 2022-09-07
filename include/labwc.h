/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_H
#define __LABWC_H
#include "config.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/util/log.h>
#if HAVE_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include <xkbcommon/xkbcommon.h>
#include "config/keybind.h"
#include "config/rcxml.h"
#include "ssd.h"
#if HAVE_NLS
#include <libintl.h>
#include <locale.h>
#define _ gettext
#else
#define _(s) (s)
#endif

#define XCURSOR_DEFAULT "left_ptr"
#define XCURSOR_SIZE 24
#define XCURSOR_MOVE "grabbing"

enum input_mode {
	LAB_INPUT_STATE_PASSTHROUGH = 0,
	LAB_INPUT_STATE_MOVE,
	LAB_INPUT_STATE_RESIZE,
	LAB_INPUT_STATE_MENU,
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

	struct wlr_drag_icon *drag_icon;
	struct wlr_pointer_constraint_v1 *current_constraint;
	struct wlr_idle *wlr_idle;
	struct wlr_idle_inhibit_manager_v1 *wlr_idle_inhibit_manager;

	/* Used to hide the workspace OSD after switching workspaces */
	struct wl_event_source *workspace_osd_timer;
	bool workspace_osd_shown_by_modifier;

	/* if set, views cannot receive focus */
	struct wlr_layer_surface_v1 *focused_layer;

	/**
	 * pressed view/surface/node will usually be NULL and is only set on
	 * button press while the mouse is over a surface and reset to NULL on
	 * button release.
	 * It is used to send cursor motion events to a surface even though
	 * the cursor has left the surface in the meantime.
	 *
	 * This allows to keep dragging a scrollbar or selecting text even
	 * when moving outside of the window.
	 */
	struct {
		struct view *view;
		struct wlr_scene_node *node;
		struct wlr_surface *surface;
	} pressed;

	struct wl_client *active_client_while_inhibited;
	struct wl_list inputs;
	struct wl_listener new_input;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_pointer_gestures_v1 *pointer_gestures;
	struct wl_listener pinch_begin;
	struct wl_listener pinch_update;
	struct wl_listener pinch_end;
	struct wl_listener swipe_begin;
	struct wl_listener swipe_update;
	struct wl_listener swipe_end;

	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;
	struct wl_listener touch_frame;

	struct wl_listener request_start_drag;
	struct wl_listener start_drag;
	struct wl_listener destroy_drag;
	struct wl_listener constraint_commit;
	struct wl_listener idle_inhibitor_create;
};

struct lab_data_buffer;
struct workspace;

struct server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;  /* Can be used for timer events */
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_backend *backend;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_layer_shell_v1 *layer_shell;

	struct wl_listener new_xdg_surface;
	struct wl_listener new_layer_surface;

	struct wl_listener xdg_toplevel_decoration;
#if HAVE_XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_ready;
	struct wl_listener new_xwayland_surface;
#endif

	struct wlr_input_inhibit_manager *input_inhibit;
	struct wl_listener input_inhibit_activate;
	struct wl_listener input_inhibit_deactivate;

	struct wl_list views;
	struct wl_list unmanaged_surfaces;

	struct seat seat;
	struct wlr_scene *scene;

	/* cursor interactive */
	enum input_mode input_mode;
	struct view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_box;
	uint32_t resize_edges;

	/* SSD state */
	struct view *focused_view;
	struct ssd_hover_state ssd_hover_state;

	/* Tree for all non-layer xdg/xwayland-shell surfaces */
	struct wlr_scene_tree *view_tree;
	/* Tree for all non-layer xdg/xwayland-shell surfaces with always-on-top */
	struct wlr_scene_tree *view_tree_always_on_top;
#if HAVE_XWAYLAND
	/* Tree for unmanaged xsurfaces without initialized view (usually popups) */
	struct wlr_scene_tree *unmanaged_tree;
#endif
	/* Tree for built in menu */
	struct wlr_scene_tree *menu_tree;

	/* Workspaces */
	struct wl_list workspaces;  /* struct workspace.link */
	struct workspace *workspace_current;
	struct workspace *workspace_last;

	struct wl_list outputs;
	struct wl_listener new_output;
	struct wlr_output_layout *output_layout;

	struct wl_listener output_layout_change;
	struct wlr_output_manager_v1 *output_manager;
	struct wl_listener output_manager_apply;
	struct wlr_output_configuration_v1 *pending_output_config;

	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

	struct wlr_drm_lease_v1_manager *drm_lease_manager;
	struct wl_listener drm_lease_request;

	struct wlr_output_power_manager_v1 *output_power_manager_v1;
	struct wl_listener output_power_manager_set_mode;

	struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
	struct wlr_pointer_constraints_v1 *constraints;
	struct wl_listener new_constraint;

	/* Set when in cycle (alt-tab) mode */
	struct osd_state {
		struct view *cycle_view;
		bool preview_was_enabled;
		struct wlr_scene_node *preview_node;
		struct wlr_scene_node *preview_anchor;
		struct multi_rect *preview_outline;
	} osd_state;

	struct theme *theme;

	struct menu *menu_current;
};

#define LAB_NR_LAYERS (4)

struct output {
	struct wl_list link; /* server::outputs */
	struct server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wl_list layers[LAB_NR_LAYERS];
	struct wlr_scene_tree *layer_tree[LAB_NR_LAYERS];
	struct wlr_scene_tree *layer_popup_tree;
	struct wlr_scene_tree *osd_tree;
	struct wlr_scene_buffer *workspace_osd;
	struct wlr_box usable_area;

	struct lab_data_buffer *osd_buffer;

	struct wl_listener destroy;
	struct wl_listener frame;

	bool leased;
};

#undef LAB_NR_LAYERS

enum view_type {
	LAB_XDG_SHELL_VIEW,
#if HAVE_XWAYLAND
	LAB_XWAYLAND_VIEW,
#endif
};

struct view_impl {
	void (*configure)(struct view *view, struct wlr_box geo);
	void (*close)(struct view *view);
	const char *(*get_string_prop)(struct view *view, const char *prop);
	void (*map)(struct view *view);
	void (*move)(struct view *view, int x, int y);
	void (*set_activated)(struct view *view, bool activated);
	void (*set_fullscreen)(struct view *view, bool fullscreen);
	void (*unmap)(struct view *view);
	void (*maximize)(struct view *view, bool maximize);
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
	struct output *output;
	struct workspace *workspace;

	union {
		struct wlr_xdg_surface *xdg_surface;
#if HAVE_XWAYLAND
		struct wlr_xwayland_surface *xwayland_surface;
#endif
	};
	struct wlr_surface *surface;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_node *scene_node;

	bool mapped;
	bool been_mapped;
	bool minimized;
	bool maximized;
	uint32_t tiled;  /* private, enum view_edge in src/view.c */
	struct wlr_output *fullscreen;

	/* geometry of the wlr_surface contained within the view */
	int x, y, w, h;

	/* user defined geometry before maximize / tiling / fullscreen */
	struct wlr_box natural_geometry;

	/*
	 * margin refers to the space between the extremities of the
	 * wlr_surface and the max extents of the server-side decorations.
	 * For xdg-shell views with CSD, this margin is zero.
	 */
	struct border margin;

	struct view_pending_move_resize {
		bool update_x, update_y;
		int x, y;
		uint32_t width, height;
		uint32_t configure_serial;
	} pending_move_resize;

	struct ssd ssd;

	struct wlr_foreign_toplevel_handle_v1 *toplevel_handle;
	struct wl_listener toplevel_handle_request_maximize;
	struct wl_listener toplevel_handle_request_minimize;
	struct wl_listener toplevel_handle_request_fullscreen;
	struct wl_listener toplevel_handle_request_activate;
	struct wl_listener toplevel_handle_request_close;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_configure;	/* xwayland only */
	struct wl_listener request_activate;
	struct wl_listener request_minimize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener set_title;
	struct wl_listener set_app_id;		/* class on xwayland */
	struct wl_listener set_decorations;	/* xwayland only */
	struct wl_listener override_redirect;	/* xwayland only */
	struct wl_listener new_popup;		/* xdg-shell only */

	/* Not (yet) implemented */
/*	struct wl_listener set_role; */
/*	struct wl_listener set_window_type; */
/*	struct wl_listener set_hints; */
};

#if HAVE_XWAYLAND
struct xwayland_unmanaged {
	struct server *server;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wlr_scene_node *node;
	struct wl_list link;

	struct wl_listener request_activate;
	struct wl_listener request_configure;
/*	struct wl_listener request_fullscreen; */
	struct wl_listener set_geometry;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener override_redirect;
};
#endif

struct constraint {
	struct seat *seat;
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
};

struct idle_inhibitor {
	struct seat *seat;
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor;
	struct wl_listener destroy;
};

void xdg_popup_create(struct view *view, struct wlr_xdg_popup *wlr_popup);

void xdg_toplevel_decoration(struct wl_listener *listener, void *data);

void xdg_surface_new(struct wl_listener *listener, void *data);

#if HAVE_XWAYLAND
void xwayland_surface_new(struct wl_listener *listener, void *data);
struct xwayland_unmanaged *xwayland_unmanaged_create(struct server *server,
	struct wlr_xwayland_surface *xsurface);
void unmanaged_handle_map(struct wl_listener *listener, void *data);
#endif

void view_set_activated(struct view *view);
void view_close(struct view *view);

/**
 * view_move_resize - resize and move view
 * @view: view to be resized and moved
 * @geo: the new geometry
 * NOTE: Only use this when the view actually changes width and/or height
 * otherwise the serials might cause a delay in moving xdg-shell clients.
 * For move only, use view_move()
 */
void view_move_resize(struct view *view, struct wlr_box geo);
void view_move(struct view *view, int x, int y);
void view_moved(struct view *view);
void view_minimize(struct view *view, bool minimized);
/* view_wlr_output - return the output that a view is mostly on */
struct wlr_output *view_wlr_output(struct view *view);
void view_center(struct view *view);
void view_maximize(struct view *view, bool maximize);
void view_set_fullscreen(struct view *view, bool fullscreen,
	struct wlr_output *wlr_output);
void view_toggle_maximize(struct view *view);
void view_toggle_decorations(struct view *view);
void view_toggle_always_on_top(struct view *view);
void view_set_decorations(struct view *view, bool decorations);
void view_toggle_fullscreen(struct view *view);
void view_adjust_for_layout_change(struct view *view);
void view_discover_output(struct view *view);
void view_move_to_edge(struct view *view, const char *direction);
void view_snap_to_edge(struct view *view, const char *direction);
const char *view_get_string_prop(struct view *view, const char *prop);
void view_update_title(struct view *view);
void view_update_app_id(struct view *view);

void view_impl_map(struct view *view);
void view_adjust_size(struct view *view, int *w, int *h);

void view_on_output_destroy(struct view *view);
void view_destroy(struct view *view);

void foreign_toplevel_handle_create(struct view *view);

/*
 * desktop.c routines deal with a collection of views
 *
 * Definition of a few keywords used in desktop.c
 *   raise    - Bring view to front.
 *   focus    - Give keyboard focus to view.
 *   activate - Set view surface as active so that client window decorations
 *              are painted to show that the window is active,typically by
 *              using a different color. Although xdg-shell protocol says you
 *              cannot assume this means that the window actually has keyboard
 *              or pointer focus, in this compositor are they called together.
 */

void desktop_move_to_front(struct view *view);
void desktop_move_to_back(struct view *view);
void desktop_focus_and_activate_view(struct seat *seat, struct view *view);
void desktop_arrange_all_views(struct server *server);

enum lab_cycle_dir {
	LAB_CYCLE_DIR_NONE,
	LAB_CYCLE_DIR_FORWARD,
	LAB_CYCLE_DIR_BACKWARD,
};

/**
 * desktop_cycle_view - return view to 'cycle' to
 * @start_view: reference point for finding next view to cycle to
 * Note: If !start_view, the second focusable view is returned
 */
struct view *desktop_cycle_view(struct server *server, struct view *start_view,
	enum lab_cycle_dir dir);
struct view *desktop_focused_view(struct server *server);
void desktop_focus_topmost_mapped_view(struct server *server);
bool isfocusable(struct view *view);

/**
 * desktop_node_and_view_at - find view and scene_node at (lx, ly)
 *
 * Behavior if node points to a surface:
 *  - If surface is a layer-surface, *view_area will be
 *    set to LAB_SSD_LAYER_SURFACE and view will be NULL.
 *
 *  - If surface is a 'lost' unmanaged xsurface (one
 *    with a never-mapped parent view), *view_area will
 *    be set to LAB_SSD_UNMANAGED and view will be NULL.
 *
 *    'Lost' unmanaged xsurfaces are usually caused by
 *    X11 applications opening popups without setting
 *    the main window as parent. Example: VLC submenus.
 *
 *  - Any other surface will cause *view_area be set to
 *    LAB_SSD_CLIENT and return the attached view.
 *
 * Behavior if node points to internal elements:
 *  - *view_area will be set to the appropiate enum value
 *    and view will be NULL if the node is not part of the SSD.
 *
 * If no node is found for the given layout coordinates,
 * *view_area will be set to LAB_SSD_ROOT and view will be NULL.
 *
 */
struct view *desktop_node_and_view_at(struct server *server, double lx,
	double ly, struct wlr_scene_node **scene_node, double *sx, double *sy,
	enum ssd_part_type *view_area);

struct view *desktop_view_at_cursor(struct server *server);

/**
 * cursor_set - set cursor icon
 * @seat - current seat
 * @cursor_name - name of cursor, for example "left_ptr" or "grab"
 */
void cursor_set(struct seat *seat, const char *cursor_name);

/**
 * cursor_update_focus - update cursor focus, may update the cursor icon
 * @server - server
 * Use it to force an update of the cursor icon and to send an enter event
 * to the surface below the cursor.
 */
void cursor_update_focus(struct server *server);

void cursor_init(struct seat *seat);
void cursor_finish(struct seat *seat);

void keyboard_init(struct seat *seat);
bool keyboard_any_modifiers_pressed(struct wlr_keyboard *keyboard);
void keyboard_finish(struct seat *seat);

void touch_init(struct seat *seat);
void touch_finish(struct seat *seat);

void seat_init(struct server *server);
void seat_finish(struct server *server);
void seat_reconfigure(struct server *server);
void seat_focus_surface(struct seat *seat, struct wlr_surface *surface);
void seat_set_focus_layer(struct seat *seat, struct wlr_layer_surface_v1 *layer);
void seat_reset_pressed(struct seat *seat);

void interactive_begin(struct view *view, enum input_mode mode,
		      uint32_t edges);
void interactive_end(struct view *view);

void output_init(struct server *server);
void output_manager_init(struct server *server);
struct output *output_from_wlr_output(struct server *server,
	struct wlr_output *wlr_output);
struct wlr_box output_usable_area_in_layout_coords(struct output *output);
struct wlr_box output_usable_area_from_cursor_coords(struct server *server);
void handle_output_power_manager_set_mode(struct wl_listener *listener,
	void *data);

void server_init(struct server *server);
void server_start(struct server *server);
void server_finish(struct server *server);

/* Updates onscreen display 'alt-tab' buffer */
void osd_update(struct server *server);
/* Closes the OSD */
void osd_finish(struct server *server);
/* Moves preview views back into their original stacking order and state */
void osd_preview_restore(struct server *server);
/* Notify OSD about a destroying view */
void osd_on_view_destroy(struct view *view);

/*
 * wlroots "input inhibitor" extension (required for swaylock) blocks
 * any client other than the requesting client from receiving events
 */
bool input_inhibit_blocks_surface(struct seat *seat,
	struct wl_resource *resource);

void create_constraint(struct wl_listener *listener, void *data);
void constrain_cursor(struct server *server, struct wlr_pointer_constraint_v1
	*constraint);

#endif /* __LABWC_H */
