/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_CURSOR_H
#define LABWC_CURSOR_H

#include <wlr/types/wlr_cursor.h>
#include <wlr/util/edges.h>
#include "ssd.h"

struct view;
struct seat;
struct server;
struct wlr_surface;
struct wlr_scene_node;
enum wl_pointer_button_state;

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
 * @cursor - name of cursor, for example LAB_CURSOR_DEFAULT or LAB_CURSOR_GRAB
 */
void cursor_set(struct seat *seat, enum lab_cursors cursor);

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
 * cursor_get_from_edge - translate wlroots edge enum to lab_cursor enum
 * @resize_edges - WLR_EDGE_ combination like WLR_EDGE_TOP | WLR_EDGE_RIGHT
 *
 * Returns LAB_CURSOR_DEFAULT on WLR_EDGE_NONE
 * Returns the appropriate lab_cursors enum if @resize_edges
 * is one of the 4 corners or one of the 4 edges.
 *
 * Asserts on invalid edge combinations like WLR_EDGE_LEFT | WLR_EDGE_RIGHT
 */
enum lab_cursors cursor_get_from_edge(uint32_t resize_edges);

/**
 * cursor_update_focus - update cursor focus, may update the cursor icon
 * @server - server
 *
 * This can be used to give the mouse focus to the surface under the cursor
 * or to force an update of the cursor icon by sending an exit and enter
 * event to an already focused surface.
 */
void cursor_update_focus(struct server *server);

/**
 * cursor_update_image - re-set the labwc cursor image
 * @seat - seat
 *
 * This can be used to update the cursor image on output scale changes.
 * If the current cursor image was not set by labwc but some client
 * this is a no-op.
 */
void cursor_update_image(struct seat *seat);

/**
 * Processes cursor motion. The return value indicates if a client
 * should be notified. Parameters sx, sy holds the surface coordinates
 * in that case.
 */
bool cursor_process_motion(struct server *server, uint32_t time, double *sx, double *sy);

/**
 * Processes cursor button press. The return value indicates if a client
 * should be notified.
 */
bool cursor_process_button_press(struct seat *seat, uint32_t button, uint32_t time_msec);

/**
 * Processes cursor button release. The return value indicates if the client
 * should be notified. Should be followed by cursor_finish_button_release()
 * after notifying a client.
 */
bool cursor_process_button_release(struct seat *seat, uint32_t button, uint32_t time_msec);

/**
 * Finishes cursor button release. The return value indicates if an interactive
 * move/resize had been finished. Should be called after notifying a client.
 */
bool cursor_finish_button_release(struct seat *seat);

void cursor_init(struct seat *seat);
void cursor_reload(struct seat *seat);
void cursor_emulate_move(struct seat *seat,
		struct wlr_input_device *device,
		double dx, double dy, uint32_t time_msec);
void cursor_emulate_move_absolute(struct seat *seat,
		struct wlr_input_device *device,
		double x, double y, uint32_t time_msec);
void cursor_emulate_button(struct seat *seat,
		uint32_t button, enum wl_pointer_button_state state, uint32_t time_msec);
void cursor_finish(struct seat *seat);

#endif /* LABWC_CURSOR_H */
