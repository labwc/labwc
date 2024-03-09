/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_H
#define LABWC_H
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
#include <wlr/types/wlr_gamma_control_v1.h>
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
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_tearing_control_v1.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/util/log.h>
#include "config/keybind.h"
#include "config/rcxml.h"
#include "input/cursor.h"
#include "input/ime.h"
#include "regions.h"
#include "session-lock.h"
#if HAVE_NLS
#include <libintl.h>
#include <locale.h>
#define _ gettext
#else
#define _(s) (s)
#endif

#define XCURSOR_DEFAULT "left_ptr"
#define XCURSOR_SIZE 24

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
	struct wl_list link; /* seat.inputs */
};

/*
 * Virtual keyboards should not belong to seat->keyboard_group. As a result we
 * need to be able to ascertain which wlr_keyboard key/modifier events come from
 * and we achieve that by using `struct keyboard` which inherits `struct input`
 * and adds keyboard specific listeners and a wlr_keyboard pointer.
 */
struct keyboard {
	struct input base;
	struct wlr_keyboard *wlr_keyboard;
	bool is_virtual;
	struct wl_listener modifier;
	struct wl_listener key;
	/* key repeat for compositor keybinds */
	uint32_t keybind_repeat_keycode;
	int32_t keybind_repeat_rate;
	struct wl_event_source *keybind_repeat;
};

struct seat {
	struct wlr_seat *seat;
	struct server *server;
	struct wlr_keyboard_group *keyboard_group;

	struct wl_list touch_points; /* struct touch_point.link */

	/*
	 * Enum of most recent server-side cursor image.  Set by
	 * cursor_set().  Cleared when a client surface is entered
	 * (in that case the client is expected to set its own cursor image).
	 */
	enum lab_cursors server_cursor;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;
	struct {
		double x, y;
	} smooth_scroll_offset;

	struct wlr_pointer_constraint_v1 *current_constraint;

	/* In support for ToggleKeybinds */
	uint32_t nr_inhibited_keybind_views;

	/* Used to hide the workspace OSD after switching workspaces */
	struct wl_event_source *workspace_osd_timer;
	bool workspace_osd_shown_by_modifier;

	/* if set, views cannot receive focus */
	struct wlr_layer_surface_v1 *focused_layer;

	struct input_method_relay *input_method_relay;

	/**
	 * pressed view/surface/node will usually be NULL and is only set on
	 * button press while the mouse is over a view or surface, and reset
	 * to NULL on button release.
	 * It is used to send cursor motion events to a surface even though
	 * the cursor has left the surface in the meantime.
	 *
	 * This allows to keep dragging a scrollbar or selecting text even
	 * when moving outside of the window.
	 *
	 * It is also used to:
	 * - determine the target view for action in "Drag" mousebind
	 * - validate view move/resize requests from CSD clients
	 *
	 * Both (view && !surface) and (surface && !view) are possible.
	 */
	struct {
		struct view *view;
		struct wlr_scene_node *node;
		struct wlr_surface *surface;
		struct wlr_surface *toplevel;
		uint32_t resize_edges;
	} pressed;

	struct {
		bool active;
		struct {
			struct wl_listener request;
			struct wl_listener start;
			struct wl_listener destroy;
		} events;
		struct wlr_scene_tree *icons;
	} drag;

	/* Private use by regions.c */
	struct region *region_active;
	struct region_overlay region_overlay;
	/* Used to prevent region snapping when starting a move with A-Left */
	bool region_prevent_snap;

	struct wl_client *active_client_while_inhibited;
	struct wl_list inputs;
	struct wl_listener new_input;
	struct wl_listener focus_change;

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
	struct wl_listener request_set_shape;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;
	struct wl_listener touch_frame;

	struct wl_listener constraint_commit;
	struct wl_listener pressed_surface_destroy;

	struct wlr_virtual_pointer_manager_v1 *virtual_pointer;
	struct wl_listener virtual_pointer_new;

	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;
	struct wl_listener virtual_keyboard_new;
};

struct lab_data_buffer;
struct workspace;

struct server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;  /* Can be used for timer events */
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_backend *backend;
	struct headless {
		struct wlr_backend *backend;
	} headless;
	struct wlr_session *session;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_layer_shell_v1 *layer_shell;

	struct wl_listener new_xdg_surface;
	struct wl_listener new_layer_surface;

	struct wl_listener kde_server_decoration;
	struct wl_listener xdg_toplevel_decoration;
#if HAVE_XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_server_ready;
	struct wl_listener xwayland_xwm_ready;
	struct wl_listener xwayland_new_surface;
#endif

	struct wlr_input_inhibit_manager *input_inhibit;
	struct wl_listener input_inhibit_activate;
	struct wl_listener input_inhibit_deactivate;

	struct wlr_xdg_activation_v1 *xdg_activation;
	struct wl_listener xdg_activation_request;

	struct wl_list views;
	struct wl_list unmanaged_surfaces;

	struct seat seat;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	/* cursor interactive */
	enum input_mode input_mode;
	struct view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_box;
	uint32_t resize_edges;

	/*
	 * 'active_view' is generally the view with keyboard-focus, updated with
	 * each "focus change". This view is drawn with "active" SSD coloring.
	 *
	 * The exception is when a layer-shell client takes keyboard-focus in
	 * which case the currently active view stays active. This is important
	 * for foreign-toplevel protocol.
	 */
	struct view *active_view;
	/*
	 * Most recently raised view. Used to avoid unnecessarily
	 * raising the same view over and over.
	 */
	struct view *last_raised_view;

	struct ssd_hover_state *ssd_hover_state;

	/* Tree for all non-layer xdg/xwayland-shell surfaces */
	struct wlr_scene_tree *view_tree;

	/*
	 * Popups need to be rendered above always-on-top views, so we reparent
	 * them to this dedicated tree
	 */
	struct wlr_scene_tree *xdg_popup_tree;

	/* Tree for all non-layer xdg/xwayland-shell surfaces with always-on-top/below */
	struct wlr_scene_tree *view_tree_always_on_top;
	struct wlr_scene_tree *view_tree_always_on_bottom;
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
	struct wl_listener output_manager_test;
	struct wl_listener output_manager_apply;
	/*
	 * While an output layout change is in process, this counter is
	 * non-zero and causes change-events from the wlr_output_layout
	 * to be ignored (to prevent, for example, moving views in a
	 * transitory layout state).  Once the counter reaches zero,
	 * do_output_layout_change() must be called explicitly.
	 */
	int pending_output_layout_change;

	struct wlr_gamma_control_manager_v1 *gamma_control_manager_v1;
	struct wl_listener gamma_control_set_gamma;

	struct session_lock *session_lock;

	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

	struct wlr_drm_lease_v1_manager *drm_lease_manager;
	struct wl_listener drm_lease_request;

	struct wlr_output_power_manager_v1 *output_power_manager_v1;
	struct wl_listener output_power_manager_set_mode;

	struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
	struct wlr_pointer_constraints_v1 *constraints;
	struct wl_listener new_constraint;

	struct wlr_tearing_control_manager_v1 *tearing_control;
	struct wl_listener tearing_new_object;

	struct wlr_input_method_manager_v2 *input_method_manager;
	struct wlr_text_input_manager_v3 *text_input_manager;

	/* Set when in cycle (alt-tab) mode */
	struct osd_state {
		struct view *cycle_view;
		bool preview_was_enabled;
		struct wlr_scene_node *preview_node;
		struct wlr_scene_tree *preview_parent;
		struct wlr_scene_node *preview_anchor;
		struct multi_rect *preview_outline;
	} osd_state;

	struct theme *theme;

	struct menu *menu_current;
	struct wl_list menus;

	pid_t primary_client_pid;
};

#define LAB_NR_LAYERS (4)

struct output {
	struct wl_list link; /* server.outputs */
	struct server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_tree *layer_tree[LAB_NR_LAYERS];
	struct wlr_scene_tree *layer_popup_tree;
	struct wlr_scene_tree *osd_tree;
	struct wlr_scene_tree *session_lock_tree;
	struct wlr_scene_buffer *workspace_osd;
	struct wlr_box usable_area;

	struct wl_list regions;  /* struct region.link */

	struct lab_data_buffer *osd_buffer;

	struct wl_listener destroy;
	struct wl_listener frame;
	struct wl_listener request_state;

	bool leased;
	bool gamma_lut_changed;
};

#undef LAB_NR_LAYERS

struct constraint {
	struct seat *seat;
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
};

void xdg_popup_create(struct view *view, struct wlr_xdg_popup *wlr_popup);
void xdg_shell_init(struct server *server);

void foreign_toplevel_handle_create(struct view *view);
void foreign_toplevel_update_outputs(struct view *view);

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

/**
 * desktop_focus_view() - do multiple things to make a view "active" and
 * ready to use:
 *  - unminimize
 *  - switch to the workspace it's on
 *  - give input (keyboard) focus
 *  - optionally raise above other views
 *
 * It's okay to call this function even if the view isn't mapped or the
 * session is locked/input is inhibited; it will simply do nothing.
 */
void desktop_focus_view(struct view *view, bool raise);

/**
 * desktop_focus_view_or_surface() - like desktop_focus_view() but can
 * also focus other (e.g. xwayland-unmanaged) surfaces
 */
void desktop_focus_view_or_surface(struct seat *seat, struct view *view,
	struct wlr_surface *surface, bool raise);

void desktop_arrange_all_views(struct server *server);
void desktop_focus_output(struct output *output);
struct view *desktop_topmost_focusable_view(struct server *server);

/**
 * Toggles the (output local) visibility of the layershell top layer
 * based on the existence of a fullscreen window on the current workspace.
 */
void desktop_update_top_layer_visiblity(struct server *server);

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

/**
 * desktop_focus_topmost_view() - focus the topmost view on the current
 * workspace, skipping views that claim not to want focus (those can
 * still be focused by explicit request, e.g. by clicking in them).
 *
 * This function is typically called when the focused view is hidden
 * (closes, is minimized, etc.) to focus the "next" view underneath.
 */
void desktop_focus_topmost_view(struct server *server);

void seat_init(struct server *server);
void seat_finish(struct server *server);
void seat_reconfigure(struct server *server);
void seat_focus_surface(struct seat *seat, struct wlr_surface *surface);

/**
 * seat_focus_lock_surface() - ONLY to be called from session-lock.c to
 * focus lock screen surfaces. Use seat_focus_surface() otherwise.
 */
void seat_focus_lock_surface(struct seat *seat, struct wlr_surface *surface);

void seat_set_focus_layer(struct seat *seat, struct wlr_layer_surface_v1 *layer);
void seat_set_pressed(struct seat *seat, struct view *view,
	struct wlr_scene_node *node, struct wlr_surface *surface,
	struct wlr_surface *toplevel, uint32_t resize_edges);
void seat_reset_pressed(struct seat *seat);
void seat_output_layout_changed(struct seat *seat);

void interactive_begin(struct view *view, enum input_mode mode, uint32_t edges);
void interactive_finish(struct view *view);
void interactive_cancel(struct view *view);

void output_init(struct server *server);
void output_manager_init(struct server *server);
struct output *output_from_wlr_output(struct server *server,
	struct wlr_output *wlr_output);
struct output *output_from_name(struct server *server, const char *name);
struct output *output_nearest_to(struct server *server, int lx, int ly);
struct output *output_nearest_to_cursor(struct server *server);
bool output_is_usable(struct output *output);
void output_update_usable_area(struct output *output);
void output_update_all_usable_areas(struct server *server, bool layout_changed);
struct wlr_box output_usable_area_in_layout_coords(struct output *output);
struct wlr_box output_usable_area_scaled(struct output *output);
void handle_output_power_manager_set_mode(struct wl_listener *listener,
	void *data);
void output_enable_adaptive_sync(struct wlr_output *output, bool enabled);
void new_tearing_hint(struct wl_listener *listener, void *data);

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

#endif /* LABWC_H */
