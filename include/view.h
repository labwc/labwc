/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_VIEW_H
#define __LABWC_VIEW_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>
#include <wlr/util/box.h>

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

struct view;
struct view_impl {
	void (*configure)(struct view *view, struct wlr_box geo);
	void (*close)(struct view *view);
	const char *(*get_string_prop)(struct view *view, const char *prop);
	void (*map)(struct view *view);
	void (*set_activated)(struct view *view, bool activated);
	void (*set_fullscreen)(struct view *view, bool fullscreen);
	void (*unmap)(struct view *view);
	void (*maximize)(struct view *view, bool maximize);
	void (*move_to_front)(struct view *view);
	void (*move_to_back)(struct view *view);
};

struct view {
	struct server *server;
	enum view_type type;
	const struct view_impl *impl;
	struct wl_list link;

	/*
	 * The output that the view is displayed on. Specifically:
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
	struct workspace *workspace;
	struct wlr_surface *surface;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_node *scene_node;

	bool mapped;
	bool been_mapped;
	bool ssd_enabled;
	enum ssd_preference ssd_preference;
	bool minimized;
	bool maximized;
	bool fullscreen;
	uint32_t tiled;  /* private, enum view_edge in src/view.c */

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

	/* used by xdg-shell views */
	uint32_t pending_configure_serial;
	struct wl_event_source *pending_configure_timeout;

	struct ssd *ssd;

	struct foreign_toplevel {
		struct wlr_foreign_toplevel_handle_v1 *handle;
		struct wl_listener maximize;
		struct wl_listener minimize;
		struct wl_listener fullscreen;
		struct wl_listener activate;
		struct wl_listener close;
		struct wl_listener destroy;
	} toplevel;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener surface_destroy;
	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_minimize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener set_title;
};

struct xdg_toplevel_view {
	struct view base;
	struct wlr_xdg_surface *xdg_surface;

	/* Events unique to xdg-toplevel views */
	struct wl_listener set_app_id;
	struct wl_listener new_popup;
};

void view_set_activated(struct view *view);
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
void view_move(struct view *view, int x, int y);
void view_moved(struct view *view);
void view_minimize(struct view *view, bool minimized);
void view_store_natural_geometry(struct view *view);

/**
 * view_center - center view within some region
 * @view: view to be centered
 * @ref: optional reference region (in layout coordinates) to center
 * within; if NULL, view is centered within usable area of its output
 */
void view_center(struct view *view, const struct wlr_box *ref);
void view_restore_to(struct view *view, struct wlr_box geometry);
void view_set_untiled(struct view *view);
void view_maximize(struct view *view, bool maximize,
	bool store_natural_geometry);
void view_set_fullscreen(struct view *view, bool fullscreen);
void view_toggle_maximize(struct view *view);
void view_toggle_decorations(struct view *view);
void view_toggle_always_on_top(struct view *view);
bool view_is_always_on_top(struct view *view);
bool view_is_tiled(struct view *view);
bool view_is_floating(struct view *view);
void view_move_to_workspace(struct view *view, struct workspace *workspace);
void view_set_decorations(struct view *view, bool decorations);
void view_toggle_fullscreen(struct view *view);
void view_adjust_for_layout_change(struct view *view);
void view_move_to_edge(struct view *view, const char *direction);
void view_snap_to_edge(struct view *view, const char *direction,
	bool store_natural_geometry);
void view_snap_to_region(struct view *view, struct region *region,
	bool store_natural_geometry);
const char *view_get_string_prop(struct view *view, const char *prop);
void view_update_title(struct view *view);
void view_update_app_id(struct view *view);
void view_reload_ssd(struct view *view);

void view_adjust_size(struct view *view, int *w, int *h);

void view_evacuate_region(struct view *view);
void view_on_output_destroy(struct view *view);
void view_destroy(struct view *view);

/* xdg.c */
struct wlr_xdg_surface *xdg_surface_from_view(struct view *view);

#endif /* __LABWC_VIEW_H */
