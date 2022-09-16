/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_CURSOR_H
#define __LABWC_CURSOR_H

#include <wlr/util/edges.h>
#include "ssd.h"

struct view;
struct seat;
struct server;
struct wlr_surface;
struct wlr_scene_node;

/* Cursors used internally by labwc */
enum lab_cursors {
	LAB_CURSOR_CLIENT = 0,
	LAB_CURSOR_DEFAULT,
	LAB_CURSOR_GRAB,
	LAB_CURSOR_RESIZE_NW,
	LAB_CURSOR_RESIZE_N,
	LAB_CURSOR_RESIZE_NE,
	LAB_CURSOR_RESIZE_E,
	LAB_CURSOR_RESIZE_SE,
	LAB_CURSOR_RESIZE_S,
	LAB_CURSOR_RESIZE_SW,
	LAB_CURSOR_RESIZE_W,
	LAB_CURSOR_COUNT
};

struct cursor_context {
	struct view *view;
	struct wlr_scene_node *node;
	struct wlr_surface *surface;
	enum ssd_part_type type;
	double sx, sy;
};

/**
 * get_cursor_context - find view and scene_node at cursor
 *
 * Behavior if node points to a surface:
 *  - If surface is a layer-surface, type will be
 *    set to LAB_SSD_LAYER_SURFACE and view will be NULL.
 *
 *  - If surface is a 'lost' unmanaged xsurface (one
 *    with a never-mapped parent view), type will
 *    be set to LAB_SSD_UNMANAGED and view will be NULL.
 *
 *    'Lost' unmanaged xsurfaces are usually caused by
 *    X11 applications opening popups without setting
 *    the main window as parent. Example: VLC submenus.
 *
 *  - Any other surface will cause type to be set to
 *    LAB_SSD_CLIENT and return the attached view.
 *
 * Behavior if node points to internal elements:
 *  - type will be set to the appropriate enum value
 *    and view will be NULL if the node is not part of the SSD.
 *
 * If no node is found for the given layout coordinates,
 * type will be set to LAB_SSD_ROOT and view will be NULL.
 *
 */
struct cursor_context get_cursor_context(struct server *server);

/**
 * cursor_set - set cursor icon
 * @seat - current seat
 * @cursor_name - name of cursor, for example "left_ptr" or "grab"
 */
void cursor_set(struct seat *seat, const char *cursor_name);

/**
 * cursor_get_resize_edges - calculate resize edge based on cursor position
 * @cursor - the current cursor (usually server->seat.cursor)
 * @cursor_context - result of get_cursor_context()
 *
 * Calculates the resize edge combination that is most appropriate based
 * on the current view and cursor position in relation to each other.
 *
 * This is mostly important when either resizing a window using a
 * keyboard modifier or when using the Resize action from a keybind.
 */
uint32_t cursor_get_resize_edges(struct wlr_cursor *cursor,
	struct cursor_context *ctx);

/**
 * cursor_update_focus - update cursor focus, may update the cursor icon
 * @server - server
 *
 * This can be used to give the mouse focus to the surface under the cursor
 * or to force an update of the cursor icon by sending an exit and enter
 * event to an already focused surface.
 */
void cursor_update_focus(struct server *server);

void cursor_init(struct seat *seat);
void cursor_finish(struct seat *seat);

#endif /* __LABWC_CURSOR_H */
