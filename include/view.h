/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_VIEW_H
#define LABWC_VIEW_H

#include "config/rcxml.h"
#include "config.h"
#include "ssd.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>
#include "common/three-state.h"

#define LAB_MIN_VIEW_HEIGHT 60

/*
 * Fallback view geometry used in some cases where a better position
 * and/or size can't be determined. Try to avoid using these except as
 * a last resort.
 */
#define VIEW_FALLBACK_X 100
#define VIEW_FALLBACK_Y 100
#define VIEW_FALLBACK_WIDTH  640
#define VIEW_FALLBACK_HEIGHT 480

/*
 * In labwc, a view is a container for surfaces which can be moved around by
 * the user. In practice this means XDG toplevel and XWayland windows.
 */

enum view_type {
	LAB_XDG_SHELL_VIEW,
#if HAVE_XWAYLAND
	LAB_XWAYLAND_VIEW,
#endif
};

enum ssd_preference {
	LAB_SSD_PREF_UNSPEC = 0,
	LAB_SSD_PREF_CLIENT,
	LAB_SSD_PREF_SERVER,
};

/**
 * Directions in which a view can be maximized. "None" is used
 * internally to mean "not maximized" but is not valid in rc.xml.
 * Therefore when parsing rc.xml, "None" means "Invalid".
 */
enum view_axis {
	VIEW_AXIS_NONE = 0,
	VIEW_AXIS_HORIZONTAL = (1 << 0),
	VIEW_AXIS_VERTICAL = (1 << 1),
	VIEW_AXIS_BOTH = (VIEW_AXIS_HORIZONTAL | VIEW_AXIS_VERTICAL),
	/*
	 * If view_axis is treated as a bitfield, INVALID should never
	 * set the HORIZONTAL or VERTICAL bits.
	 */
	VIEW_AXIS_INVALID = (1 << 2),
};

enum view_edge {
	VIEW_EDGE_INVALID = 0,

	VIEW_EDGE_LEFT,
	VIEW_EDGE_RIGHT,
	VIEW_EDGE_UP,
	VIEW_EDGE_DOWN,
	VIEW_EDGE_CENTER,
};

enum view_wants_focus {
	/* View does not want focus */
	VIEW_WANTS_FOCUS_NEVER = 0,
	/* View wants focus */
	VIEW_WANTS_FOCUS_ALWAYS,
	/*
	 * View should be offered focus and may accept or decline
	 * (a.k.a. ICCCM Globally Active input model). Labwc generally
	 * avoids focusing these views automatically (e.g. when another
	 * view on top is closed) but they may be focused by user action
	 * (e.g. mouse click).
	 */
	VIEW_WANTS_FOCUS_OFFER,
};

enum window_type {
	/* https://specifications.freedesktop.org/wm-spec/wm-spec-1.4.html#idm45649101374512 */
	NET_WM_WINDOW_TYPE_DESKTOP = 0,
	NET_WM_WINDOW_TYPE_DOCK,
	NET_WM_WINDOW_TYPE_TOOLBAR,
	NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_UTILITY,
	NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_DIALOG,
	NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	NET_WM_WINDOW_TYPE_POPUP_MENU,
	NET_WM_WINDOW_TYPE_TOOLTIP,
	NET_WM_WINDOW_TYPE_NOTIFICATION,
	NET_WM_WINDOW_TYPE_COMBO,
	NET_WM_WINDOW_TYPE_DND,
	NET_WM_WINDOW_TYPE_NORMAL,

	WINDOW_TYPE_LEN
};

struct view;
struct wlr_surface;
struct foreign_toplevel;

/* Common to struct view and struct xwayland_unmanaged */
struct mappable {
	bool connected;
	struct wl_listener map;
	struct wl_listener unmap;
};

/* Basic size hints (subset of XSizeHints from X11) */
struct view_size_hints {
	int min_width;
	int min_height;
	int width_inc;
	int height_inc;
	int base_width;
	int base_height;
};

struct view_impl {
	void (*configure)(struct view *view, struct wlr_box geo);
	void (*close)(struct view *view);
	const char *(*get_string_prop)(struct view *view, const char *prop);
	void (*map)(struct view *view);
	void (*set_activated)(struct view *view, bool activated);
	void (*set_fullscreen)(struct view *view, bool fullscreen);
	void (*notify_tiled)(struct view *view);
	/*
	 * client_request is true if the client unmapped its own
	 * surface; false if we are just minimizing the view. The two
	 * cases are similar but have subtle differences (e.g., when
	 * minimizing we don't destroy the foreign toplevel handle).
	 */
	void (*unmap)(struct view *view, bool client_request);
	void (*maximize)(struct view *view, bool maximize);
	void (*minimize)(struct view *view, bool minimize);
	void (*move_to_front)(struct view *view);
	void (*move_to_back)(struct view *view);
	void (*shade)(struct view *view, bool shaded);
	struct view *(*get_root)(struct view *self);
	void (*append_children)(struct view *self, struct wl_array *children);
	struct view_size_hints (*get_size_hints)(struct view *self);
	/* if not implemented, VIEW_WANTS_FOCUS_ALWAYS is assumed */
	enum view_wants_focus (*wants_focus)(struct view *self);
	/* returns true if view reserves space at screen edge */
	bool (*has_strut_partial)(struct view *self);
	/* returns true if view declared itself a window type */
	bool (*contains_window_type)(struct view *view, int32_t window_type);
	/* returns the client pid that this view belongs to */
	pid_t (*get_pid)(struct view *view);
};

struct view {
	struct server *server;
	enum view_type type;
	const struct view_impl *impl;
	struct wl_list link;

	/*
	 * The primary output that the view is displayed on. Specifically:
	 *
	 *  - For floating views, this is the output nearest to the
	 *    center of the view. It is computed automatically when the
	 *    view is moved or the output layout changes.
	 *
	 *  - For fullscreen/maximized/tiled views, this is the output
	 *    used to compute the view's geometry. The view remains on
	 *    the same output unless it is disabled or disconnected.
	 *
	 * Many view functions (e.g. view_center(), view_fullscreen(),
	 * view_maximize(), etc.) allow specifying a particular output
	 * by calling view_set_output() beforehand.
	 */
	struct output *output;

	/*
	 * The outputs that the view is displayed on.
	 * This is used to notify the foreign toplevel
	 * implementation and to update the SSD invisible
	 * resize area.
	 * It is a bitset of output->scene_output->index.
	 */
	uint64_t outputs;

	struct workspace *workspace;
	struct wlr_surface *surface;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_node *content_node;

	bool mapped;
	bool been_mapped;
	bool ssd_enabled;
	bool ssd_titlebar_hidden;
	enum ssd_preference ssd_preference;
	bool shaded;
	bool minimized;
	enum view_axis maximized;
	bool fullscreen;
	bool tearing_hint;
	enum three_state force_tearing;
	bool visible_on_all_workspaces;
	enum view_edge tiled;
	uint32_t edges_visible;  /* enum wlr_edges bitset */
	bool inhibits_keybinds;
	xkb_layout_index_t keyboard_layout;

	/* Pointer to an output owned struct region, may be NULL */
	struct region *tiled_region;
	/* Set to region->name when tiled_region is free'd by a destroying output */
	char *tiled_region_evacuate;

	/*
	 * Geometry of the wlr_surface contained within the view, as
	 * currently displayed. Should be kept in sync with the
	 * scene-graph at all times.
	 */
	struct wlr_box current;
	/*
	 * Expected geometry after any pending move/resize requests
	 * have been processed. Should match current geometry when no
	 * move/resize requests are pending.
	 */
	struct wlr_box pending;
	/*
	 * Saved geometry which will be restored when the view returns
	 * to normal/floating state after being maximized/fullscreen/
	 * tiled. Values are undefined/out-of-date when the view is not
	 * maximized/fullscreen/tiled.
	 */
	struct wlr_box natural_geometry;
	/*
	 * Whenever an output layout change triggers a view relocation, the
	 * last pending position (or natural geometry) will be saved so the
	 * view may be restored to its original location on a subsequent layout
	 * change.
	 */
	struct wlr_box last_layout_geometry;

	/* used by xdg-shell views */
	uint32_t pending_configure_serial;
	struct wl_event_source *pending_configure_timeout;

	struct ssd *ssd;
	struct resize_indicator {
		int width, height;
		struct wlr_scene_tree *tree;
		struct wlr_scene_rect *border;
		struct wlr_scene_rect *background;
		struct scaled_font_buffer *text;
	} resize_indicator;
	struct resize_outlines {
		struct wlr_box view_geo;
		struct multi_rect *rect;
	} resize_outlines;

	struct mappable mappable;

	struct wl_listener destroy;
	struct wl_listener surface_destroy;
	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_minimize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener set_title;

	struct foreign_toplevel *foreign_toplevel;

	struct {
		struct wl_signal new_app_id;
		struct wl_signal new_title;
		struct wl_signal new_outputs;
		struct wl_signal maximized;
		struct wl_signal minimized;
		struct wl_signal fullscreened;
		struct wl_signal activated;     /* bool *activated */
	} events;
};

struct view_query {
	struct wl_list link;
	char *identifier;
	char *title;
	int window_type;
	char *sandbox_engine;
	char *sandbox_app_id;
	enum three_state shaded;
	enum view_axis maximized;
	enum three_state iconified;
	enum three_state focused;
	enum three_state omnipresent;
	enum view_edge tiled;
	char *tiled_region;
	char *desktop;
	enum ssd_mode decoration;
	char *monitor;
};

struct xdg_toplevel_view {
	struct view base;
	struct wlr_xdg_surface *xdg_surface;

	/* Events unique to xdg-toplevel views */
	struct wl_listener set_app_id;
	struct wl_listener request_show_window_menu;
	struct wl_listener new_popup;
};

/* All criteria is applied in AND logic */
enum lab_view_criteria {
	/* No filter -> all focusable views */
	LAB_VIEW_CRITERIA_NONE = 0,

	/*
	 * Includes always-on-top views, e.g.
	 * what is visible on the current workspace
	 */
	LAB_VIEW_CRITERIA_CURRENT_WORKSPACE       = 1 << 0,

	/* Positive criteria */
	LAB_VIEW_CRITERIA_FULLSCREEN              = 1 << 1,
	LAB_VIEW_CRITERIA_ALWAYS_ON_TOP           = 1 << 2,
	LAB_VIEW_CRITERIA_ROOT_TOPLEVEL           = 1 << 3,

	/* Negative criteria */
	LAB_VIEW_CRITERIA_NO_ALWAYS_ON_TOP        = 1 << 6,
	LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER = 1 << 7,
};

/**
 * view_from_wlr_surface() - returns the view associated with a
 * wlr_surface, or NULL if the surface has no associated view.
 */
struct view *view_from_wlr_surface(struct wlr_surface *surface);

/**
 * view_query_create() - Create a new heap allocated view query with
 * all members initialized to their default values (window_type = -1,
 * NULL for strings)
 */
struct view_query *view_query_create(void);

/**
 * view_query_free() - Free a given view query
 * @query: Query to be freed.
 */
void view_query_free(struct view_query *view);

/**
 * view_matches_query() - Check if view matches the given criteria
 * @view: View to checked.
 * @query: Criteria to match against.
 *
 * Returns true if %view matches all of the criteria given in %query, false
 * otherwise.
 */
bool view_matches_query(struct view *view, struct view_query *query);

/**
 * for_each_view() - iterate over all views which match criteria
 * @view: Iterator.
 * @head: Head of list to iterate over.
 * @criteria: Criteria to match against.
 * Example:
 *	struct view *view;
 *	for_each_view(view, &server->views, LAB_VIEW_CRITERIA_NONE) {
 *		printf("%s\n", view_get_string_prop(view, "app_id"));
 *	}
 */
#define for_each_view(view, head, criteria)           \
	for (view = view_next(head, NULL, criteria);  \
	     view;                                    \
	     view = view_next(head, view, criteria))

/**
 * for_each_view_reverse() - iterate over all views which match criteria
 * @view: Iterator.
 * @head: Head of list to iterate over.
 * @criteria: Criteria to match against.
 * Example:
 *	struct view *view;
 *	for_each_view_reverse(view, &server->views, LAB_VIEW_CRITERIA_NONE) {
 *		printf("%s\n", view_get_string_prop(view, "app_id"));
 *	}
 */
#define for_each_view_reverse(view, head, criteria)   \
	for (view = view_prev(head, NULL, criteria);  \
	     view;                                    \
	     view = view_prev(head, view, criteria))

/**
 * view_next() - Get next view which matches criteria.
 * @head: Head of list to iterate over.
 * @view: Current view from which to find the next one. If NULL is provided as
 *	  the view argument, the start of the list will be used.
 * @criteria: Criteria to match against.
 *
 * Returns NULL if there are no views matching the criteria.
 */
struct view *view_next(struct wl_list *head, struct view *view,
	enum lab_view_criteria criteria);

/**
 * view_prev() - Get previous view which matches criteria.
 * @head: Head of list to iterate over.
 * @view: Current view from which to find the previous one. If NULL is provided
 *        as the view argument, the end of the list will be used.
 * @criteria: Criteria to match against.
 *
 * Returns NULL if there are no views matching the criteria.
 */
struct view *view_prev(struct wl_list *head, struct view *view,
	enum lab_view_criteria criteria);

/*
 * Same as `view_next()` except that they iterate one whole cycle rather than
 * stopping at the list-head
 */
struct view *view_next_no_head_stop(struct wl_list *head, struct view *from,
	enum lab_view_criteria criteria);
struct view *view_prev_no_head_stop(struct wl_list *head, struct view *from,
	enum lab_view_criteria criteria);

/**
 * view_array_append() - Append views that match criteria to array
 * @server: server context
 * @views: arrays to append to
 * @criteria: criteria to match against
 *
 * This function is useful in cases where the calling function may change the
 * stacking order or where it needs to iterate over the views multiple times,
 * for example to get the number of views before processing them.
 *
 * Note: This array has a very short shelf-life so it is intended to be used
 *       with a single-use-throw-away approach.
 *
 * Example usage:
 *	struct view **view;
 *	struct wl_array views;
 *	wl_array_init(&views);
 *	view_array_append(server, &views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE);
 *	wl_array_for_each(view, &views) {
 *		// Do something with *view
 *	}
 *	wl_array_release(&views);
 */
void view_array_append(struct server *server, struct wl_array *views,
	enum lab_view_criteria criteria);

enum view_wants_focus view_wants_focus(struct view *view);
bool view_contains_window_type(struct view *view, enum window_type window_type);

/**
 * view_edge_invert() - select the opposite of a provided edge
 *
 * VIEW_EDGE_CENTER and VIEW_EDGE_INVALID both map to VIEW_EDGE_INVALID.
 *
 * @edge: edge to be inverted
 */
enum view_edge view_edge_invert(enum view_edge edge);

/**
 * view_is_focusable() - Check whether or not a view can be focused
 * @view: view to be checked
 *
 * The purpose of this test is to filter out views (generally Xwayland) which
 * are not meant to be focused such as those with surfaces
 *	a. that have been created but never mapped;
 *	b. set to NULL after client minimize-request.
 *
 * The only views that are allowed to be focused are those that have a surface
 * and have been mapped at some point since creation.
 */
bool view_is_focusable(struct view *view);

void mappable_connect(struct mappable *mappable, struct wlr_surface *surface,
	wl_notify_func_t notify_map, wl_notify_func_t notify_unmap);
void mappable_disconnect(struct mappable *mappable);

void view_toggle_keybinds(struct view *view);

void view_set_activated(struct view *view, bool activated);
void view_set_output(struct view *view, struct output *output);
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
void view_resize_relative(struct view *view,
	int left, int right, int top, int bottom);
void view_move_relative(struct view *view, int x, int y);
void view_move(struct view *view, int x, int y);
void view_move_to_cursor(struct view *view);
void view_moved(struct view *view);
void view_minimize(struct view *view, bool minimized);
bool view_compute_centered_position(struct view *view,
	const struct wlr_box *ref, int w, int h, int *x, int *y);
void view_set_fallback_natural_geometry(struct view *view);
void view_store_natural_geometry(struct view *view);

/**
 * view_apply_natural_geometry - adjust view->natural_geometry if it doesn't
 * intersect with view->output and then apply it
 */
void view_apply_natural_geometry(struct view *view);

/**
 * view_effective_height - effective height of view, with respect to shaded state
 * @view: view for which effective height is desired
 * @use_pending: if false, report current height; otherwise, report pending height
 */
int view_effective_height(struct view *view, bool use_pending);

/**
 * view_center - center view within some region
 * @view: view to be centered
 * @ref: optional reference region (in layout coordinates) to center
 * within; if NULL, view is centered within usable area of its output
 */
void view_center(struct view *view, const struct wlr_box *ref);

/**
 * view_place_by_policy - apply placement strategy to view
 * @view: view to be placed
 * @allow_cursor: set to false to ignore center-on-cursor policy
 * @policy: placement policy to apply
 */
void view_place_by_policy(struct view *view, bool allow_cursor,
	enum view_placement_policy policy);
void view_constrain_size_to_that_of_usable_area(struct view *view);

void view_restore_to(struct view *view, struct wlr_box geometry);
void view_set_untiled(struct view *view);
void view_maximize(struct view *view, enum view_axis axis,
	bool store_natural_geometry);
void view_set_fullscreen(struct view *view, bool fullscreen);
void view_toggle_maximize(struct view *view, enum view_axis axis);
bool view_wants_decorations(struct view *view);
void view_toggle_decorations(struct view *view);

bool view_is_always_on_top(struct view *view);
bool view_is_always_on_bottom(struct view *view);
bool view_is_omnipresent(struct view *view);
void view_toggle_always_on_top(struct view *view);
void view_toggle_always_on_bottom(struct view *view);
void view_toggle_visible_on_all_workspaces(struct view *view);

bool view_is_tiled(struct view *view);
bool view_is_tiled_and_notify_tiled(struct view *view);
bool view_is_floating(struct view *view);
void view_move_to_workspace(struct view *view, struct workspace *workspace);
enum ssd_mode view_get_ssd_mode(struct view *view);
void view_set_ssd_mode(struct view *view, enum ssd_mode mode);
void view_set_decorations(struct view *view, enum ssd_mode mode, bool force_ssd);
void view_toggle_fullscreen(struct view *view);
void view_invalidate_last_layout_geometry(struct view *view);
void view_adjust_for_layout_change(struct view *view);
void view_move_to_edge(struct view *view, enum view_edge direction, bool snap_to_windows);
void view_grow_to_edge(struct view *view, enum view_edge direction);
void view_shrink_to_edge(struct view *view, enum view_edge direction);
void view_snap_to_edge(struct view *view, enum view_edge direction,
	bool across_outputs, bool store_natural_geometry);
void view_snap_to_region(struct view *view, struct region *region, bool store_natural_geometry);
void view_move_to_output(struct view *view, struct output *output);

void view_move_to_front(struct view *view);
void view_move_to_back(struct view *view);
struct view *view_get_root(struct view *view);
void view_append_children(struct view *view, struct wl_array *children);
bool view_on_output(struct view *view, struct output *output);

/**
 * view_has_strut_partial() - returns true for views that reserve space
 * at a screen edge (e.g. panels). These views are treated as if they
 * have the fixedPosition window rule: i.e. they are not restricted to
 * the usable area and cannot be moved/resized interactively.
 */
bool view_has_strut_partial(struct view *view);

const char *view_get_string_prop(struct view *view, const char *prop);
void view_update_title(struct view *view);
void view_update_app_id(struct view *view);
void view_reload_ssd(struct view *view);
int view_get_min_width(void);

void view_set_shade(struct view *view, bool shaded);

struct view_size_hints view_get_size_hints(struct view *view);
void view_adjust_size(struct view *view, int *w, int *h);

void view_evacuate_region(struct view *view);
void view_on_output_destroy(struct view *view);
void view_connect_map(struct view *view, struct wlr_surface *surface);

void view_init(struct view *view);
void view_destroy(struct view *view);

enum view_axis view_axis_parse(const char *direction);
enum view_edge view_edge_parse(const char *direction);
enum view_placement_policy view_placement_parse(const char *policy);

/* xdg.c */
struct wlr_xdg_surface *xdg_surface_from_view(struct view *view);

#endif /* LABWC_VIEW_H */
