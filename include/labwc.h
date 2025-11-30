/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_H
#define LABWC_H
#include "config.h"
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/set.h"
#include "input/cursor.h"
#include "overlay.h"

#define XCURSOR_DEFAULT "left_ptr"
#define XCURSOR_SIZE 24

struct wlr_xdg_popup;

enum input_mode {
	LAB_INPUT_STATE_PASSTHROUGH = 0,
	LAB_INPUT_STATE_MOVE,
	LAB_INPUT_STATE_RESIZE,
	LAB_INPUT_STATE_MENU,
	LAB_INPUT_STATE_WINDOW_SWITCHER,
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
	bool cursor_visible;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;
	struct accumulated_scroll {
		double delta;
		double delta_discrete;
	} accumulated_scrolls[2]; /* indexed by wl_pointer_axis */
	bool cursor_scroll_wheel_emulation;

	/*
	 * The surface whose keyboard focus is temporarily cleared with
	 * seat_focus_override_begin() and restored with
	 * seat_focus_override_end().
	 */
	struct {
		struct wlr_surface *surface;
		struct wl_listener surface_destroy;
	} focus_override;

	struct wlr_pointer_constraint_v1 *current_constraint;

	/* Used to hide the workspace OSD after switching workspaces */
	struct wl_event_source *workspace_osd_timer;
	bool workspace_osd_shown_by_modifier;

	/* if set, views cannot receive focus */
	struct wlr_layer_surface_v1 *focused_layer;

	struct input_method_relay *input_method_relay;

	/**
	 * Cursor context saved when a mouse button is pressed on a view/surface.
	 * It is used to send cursor motion events to a surface even though
	 * the cursor has left the surface in the meantime.
	 *
	 * This allows to keep dragging a scrollbar or selecting text even
	 * when moving outside of the window.
	 *
	 * It is also used to:
	 * - determine the target view for action in "Drag" mousebind
	 * - validate view move/resize requests from CSD clients
	 */
	struct cursor_context_saved pressed;

	/* Cursor context of the last cursor motion */
	struct cursor_context_saved last_cursor_ctx;

	struct lab_set bound_buttons;

	struct {
		bool active;
		struct {
			struct wl_listener request;
			struct wl_listener start;
			struct wl_listener destroy;
		} events;
		struct wlr_scene_tree *icons;
	} drag;

	struct overlay overlay;
	/* Used to prevent region snapping when starting a move with A-Left */
	bool region_prevent_snap;

	struct wl_list inputs;
	struct wl_listener new_input;
	struct wl_listener focus_change;

	struct {
		struct wl_listener motion;
		struct wl_listener motion_absolute;
		struct wl_listener button;
		struct wl_listener axis;
		struct wl_listener frame;
	} on_cursor;

	struct wlr_pointer_gestures_v1 *pointer_gestures;
	struct wl_listener pinch_begin;
	struct wl_listener pinch_update;
	struct wl_listener pinch_end;
	struct wl_listener swipe_begin;
	struct wl_listener swipe_update;
	struct wl_listener swipe_end;
	struct wl_listener hold_begin;
	struct wl_listener hold_end;

	struct wl_listener request_set_cursor;
	struct wl_listener request_set_shape;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;
	struct wl_listener touch_frame;

	struct wl_listener tablet_tool_proximity;
	struct wl_listener tablet_tool_axis;
	struct wl_listener tablet_tool_tip;
	struct wl_listener tablet_tool_button;

	struct wl_list tablets;
	struct wl_list tablet_tools;
	struct wl_list tablet_pads;

	struct wl_listener constraint_commit;

	struct wlr_virtual_pointer_manager_v1 *virtual_pointer;
	struct wl_listener new_virtual_pointer;

	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;
	struct wl_listener new_virtual_keyboard;
};

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
	struct wlr_linux_dmabuf_v1 *linux_dmabuf;
	struct wlr_compositor *compositor;

	struct wl_event_source *sighup_source;
	struct wl_event_source *sigint_source;
	struct wl_event_source *sigterm_source;
	struct wl_event_source *sigchld_source;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_layer_shell_v1 *layer_shell;

	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_layer_surface;

	struct wl_listener kde_server_decoration;
	struct wl_listener xdg_toplevel_decoration;
#if HAVE_XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_server_ready;
	struct wl_listener xwayland_xwm_ready;
	struct wl_listener xwayland_new_surface;
#endif

	struct wlr_xdg_activation_v1 *xdg_activation;
	struct wl_listener xdg_activation_request;
	struct wl_listener xdg_activation_new_token;

	struct wlr_xdg_toplevel_icon_manager_v1 *xdg_toplevel_icon_manager;
	struct wl_listener xdg_toplevel_icon_set_icon;

	struct wl_list views;
	struct wl_list unmanaged_surfaces;

	struct seat seat;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	bool direct_scanout_enabled;

	/* cursor interactive */
	enum input_mode input_mode;
	struct view *grabbed_view;
	/* Cursor position when interactive move/resize is requested */
	double grab_x, grab_y;
	/* View geometry when interactive move/resize is requested */
	struct wlr_box grab_box;
	enum lab_edge resize_edges;

	/*
	 * 'active_view' is generally the view with keyboard-focus, updated with
	 * each "focus change". This view is drawn with "active" SSD coloring.
	 *
	 * The exceptions are:
	 * - when a layer-shell client takes keyboard-focus in which case the
	 *   currently active view stays active
	 * - when keyboard focus is temporarily cleared for server-side
	 *   interactions like Move/Resize, window switcher and menus.
	 *
	 * Note that active_view is synced with foreign-toplevel clients.
	 */
	struct view *active_view;

	struct ssd_button *hovered_button;

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
	struct {
		struct wl_list all;  /* struct workspace.link */
		struct workspace *current;
		struct workspace *last;
		struct lab_cosmic_workspace_manager *cosmic_manager;
		struct lab_cosmic_workspace_group *cosmic_group;
		struct lab_ext_workspace_manager *ext_manager;
		struct lab_ext_workspace_group *ext_group;
		struct {
			struct wl_listener layout_output_added;
		} on;
	} workspaces;

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

	struct wl_listener renderer_lost;

	struct wlr_gamma_control_manager_v1 *gamma_control_manager_v1;
	struct wl_listener gamma_control_set_gamma;

	struct session_lock_manager *session_lock_manager;

	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
	struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;

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

	struct wlr_tablet_manager_v2 *tablet_manager;
	struct wlr_security_context_manager_v1 *security_context_manager_v1;

	/* Set when in cycle (alt-tab) mode */
	struct osd_state {
		struct view *cycle_view;
		bool preview_was_shaded;
		bool preview_was_enabled;
		struct wlr_scene_node *preview_node;
		struct wlr_scene_tree *preview_parent;
		struct wlr_scene_node *preview_anchor;
		struct lab_scene_rect *preview_outline;
	} osd_state;

	struct theme *theme;

	struct menu *menu_current;
	struct wl_list menus;

	struct sfdo *sfdo;

	pid_t primary_client_pid;
};

void xdg_popup_create(struct view *view, struct wlr_xdg_popup *wlr_popup);
void xdg_shell_init(struct server *server);
void xdg_shell_finish(struct server *server);

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
 * session is locked; it will simply do nothing.
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

/**
 * Toggles the (output local) visibility of the layershell top layer
 * based on the existence of a fullscreen window on the current workspace.
 */
void desktop_update_top_layer_visibility(struct server *server);

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

void seat_pointer_end_grab(struct seat *seat, struct wlr_surface *surface);

/**
 * seat_focus_lock_surface() - ONLY to be called from session-lock.c to
 * focus lock screen surfaces. Use seat_focus_surface() otherwise.
 */
void seat_focus_lock_surface(struct seat *seat, struct wlr_surface *surface);

void seat_set_focus_layer(struct seat *seat, struct wlr_layer_surface_v1 *layer);
void seat_output_layout_changed(struct seat *seat);

/*
 * Temporarily clear the pointer/keyboard focus from the client at the
 * beginning of interactive move/resize, window switcher or menu interactions.
 * The focus is kept cleared until seat_focus_override_end() is called or
 * layer-shell/session-lock surfaces are mapped.
 */
void seat_focus_override_begin(struct seat *seat, enum input_mode input_mode,
	enum lab_cursors cursor_shape);
/*
 * Restore the pointer/keyboard focus which was cleared in
 * seat_focus_override_begin().
 */
void seat_focus_override_end(struct seat *seat);

/**
 * interactive_anchor_to_cursor() - repositions the geometry to remain
 * underneath the cursor when its size changes during interactive move.
 * This function also resizes server->grab_box and repositions it to remain
 * underneath server->grab_{x,y}.
 *
 * geo->{width,height} are provided by the caller.
 * geo->{x,y} are computed by this function.
 */
void interactive_anchor_to_cursor(struct server *server, struct wlr_box *geo);

void interactive_begin(struct view *view, enum input_mode mode,
	enum lab_edge edges);
void interactive_finish(struct view *view);
void interactive_cancel(struct view *view);

/**
 * Returns the edge to snap a window to.
 * For example, if the output-relative cursor position (x,y) fulfills
 * x <= (<snapping><cornerRange>) and y <= (<snapping><range>),
 * then edge1=LAB_EDGE_TOP and edge2=LAB_EDGE_LEFT.
 * The value of (edge1|edge2) can be passed to view_snap_to_edge().
 */
bool edge_from_cursor(struct seat *seat, struct output **dest_output,
	enum lab_edge *edge1, enum lab_edge *edge2);

void handle_tearing_new_object(struct wl_listener *listener, void *data);

void server_init(struct server *server);
void server_start(struct server *server);
void server_finish(struct server *server);

void create_constraint(struct wl_listener *listener, void *data);
void constrain_cursor(struct server *server, struct wlr_pointer_constraint_v1
	*constraint);

#endif /* LABWC_H */
