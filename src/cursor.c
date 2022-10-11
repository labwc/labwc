// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <linux/input-event-codes.h>
#include <sys/time.h>
#include <time.h>
#include <wlr/types/wlr_primary_selection.h>
#include "action.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "config/mousebind.h"
#include "labwc.h"
#include "menu/menu.h"
#include "resistance.h"
#include "ssd.h"

static const char **cursor_names = NULL;

/* Usual cursor names */
static const char *cursors_xdg[] = {
	NULL,
	"default",
	"grab",
	"nw-resize",
	"n-resize",
	"ne-resize",
	"e-resize",
	"se-resize",
	"s-resize",
	"sw-resize",
	"w-resize"
};

/* XCursor fallbacks */
static const char *cursors_x11[] = {
	NULL,
	"left_ptr",
	"grabbing",
	"top_left_corner",
	"top_side",
	"top_right_corner",
	"right_side",
	"bottom_right_corner",
	"bottom_side",
	"bottom_left_corner",
	"left_side"
};

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
static_assert(
	ARRAY_SIZE(cursors_xdg) == LAB_CURSOR_COUNT,
	"XDG cursor names are out ot sync");
static_assert(
	ARRAY_SIZE(cursors_x11) == LAB_CURSOR_COUNT,
	"X11 cursor names are out ot sync");
#undef ARRAY_SIZE

enum lab_cursors
cursor_get_from_edge(uint32_t resize_edges)
{
	switch (resize_edges) {
	case WLR_EDGE_NONE:
		return LAB_CURSOR_DEFAULT;
	case WLR_EDGE_TOP | WLR_EDGE_LEFT:
		return LAB_CURSOR_RESIZE_NW;
	case WLR_EDGE_TOP:
		return LAB_CURSOR_RESIZE_N;
	case WLR_EDGE_TOP | WLR_EDGE_RIGHT:
		return LAB_CURSOR_RESIZE_NE;
	case WLR_EDGE_RIGHT:
		return LAB_CURSOR_RESIZE_E;
	case WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT:
		return LAB_CURSOR_RESIZE_SE;
	case WLR_EDGE_BOTTOM:
		return LAB_CURSOR_RESIZE_S;
	case WLR_EDGE_BOTTOM | WLR_EDGE_LEFT:
		return LAB_CURSOR_RESIZE_SW;
	case WLR_EDGE_LEFT:
		return LAB_CURSOR_RESIZE_W;
	default:
		wlr_log(WLR_ERROR,
			"Failed to resolve wlroots edge %u to cursor name", resize_edges);
		assert(false);
	}
}

static enum lab_cursors
cursor_get_from_ssd(enum ssd_part_type view_area)
{
	uint32_t resize_edges = ssd_resize_edges(view_area);
	return cursor_get_from_edge(resize_edges);
}

static struct wlr_surface *
get_toplevel(struct wlr_surface *surface)
{
	while (surface && wlr_surface_is_xdg_surface(surface)) {
		struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_from_wlr_surface(surface);
		if (!xdg_surface) {
			return NULL;
		}

		switch (xdg_surface->role) {
		case WLR_XDG_SURFACE_ROLE_NONE:
			return NULL;
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
			return surface;
		case WLR_XDG_SURFACE_ROLE_POPUP:
			surface = xdg_surface->popup->parent;
			continue;
		}
	}
	if (surface && wlr_surface_is_layer_surface(surface)) {
		return surface;
	}
	return NULL;
}

static void
request_cursor_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, request_cursor);

	if (seat->server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		/* Prevent setting a cursor image when moving or resizing */
		return;
	}

	/*
	 * This event is raised by the seat when a client provides a cursor
	 * image
	 */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		seat->seat->pointer_state.focused_client;

	/*
	 * This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first.
	 */
	if (focused_client == event->seat_client) {
		/*
		 * Once we've vetted the client, we can tell the cursor to use
		 * the provided surface as the cursor image. It will set the
		 * hardware cursor on the output that it's currently on and
		 * continue to do so as the cursor moves between outputs.
		 */

		wlr_cursor_set_surface(seat->cursor, event->surface,
			event->hotspot_x, event->hotspot_y);
	}
}

static void
request_set_selection_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(
		listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat->seat, event->source,
		event->serial);
}

static void
request_set_primary_selection_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(
		listener, seat, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat->seat, event->source,
		event->serial);
}

static void
request_start_drag_notify(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(
		listener, seat, request_start_drag);
	struct wlr_seat_request_start_drag_event *event = data;
	if (wlr_seat_validate_pointer_grab_serial(seat->seat, event->origin,
			event->serial)) {
		wlr_seat_start_pointer_drag(seat->seat, event->drag,
			event->serial);
	} else {
		wlr_data_source_destroy(event->drag->source);
	}
}

static void
process_cursor_move(struct server *server, uint32_t time)
{
	double dx = server->seat.cursor->x - server->grab_x;
	double dy = server->seat.cursor->y - server->grab_y;
	struct view *view = server->grabbed_view;

	/* Move the grabbed view to the new position. */
	dx += server->grab_box.x;
	dy += server->grab_box.y;
	resistance_move_apply(view, &dx, &dy);
	view_move(view, dx, dy);
}

static void
process_cursor_resize(struct server *server, uint32_t time)
{
	double dx = server->seat.cursor->x - server->grab_x;
	double dy = server->seat.cursor->y - server->grab_y;

	struct view *view = server->grabbed_view;
	struct wlr_box new_view_geo = {
		.x = view->x, .y = view->y, .width = view->w, .height = view->h
	};

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_view_geo.height = server->grab_box.height - dy;
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_view_geo.height = server->grab_box.height + dy;
	}

	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_view_geo.width = server->grab_box.width - dx;
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_view_geo.width = server->grab_box.width + dx;
	}

	resistance_resize_apply(view, &new_view_geo);
	view_adjust_size(view, &new_view_geo.width, &new_view_geo.height);

	if (server->resize_edges & WLR_EDGE_TOP) {
		/* anchor bottom edge */
		new_view_geo.y = server->grab_box.y +
			server->grab_box.height - new_view_geo.height;
	}

	if (server->resize_edges & WLR_EDGE_LEFT) {
		/* anchor right edge */
		new_view_geo.x = server->grab_box.x +
			server->grab_box.width - new_view_geo.width;
	}

	view_move_resize(view, new_view_geo);
}

void
cursor_set(struct seat *seat, enum lab_cursors cursor)
{
	assert(cursor > LAB_CURSOR_CLIENT && cursor < LAB_CURSOR_COUNT);

	/* Prevent setting the same cursor image twice */
	if (seat->server_cursor == cursor) {
		return;
	}

	wlr_xcursor_manager_set_cursor_image(
		seat->xcursor_manager, cursor_names[cursor], seat->cursor);
	seat->server_cursor = cursor;
}

bool
input_inhibit_blocks_surface(struct seat *seat, struct wl_resource *resource)
{
	struct wl_client *inhibiting_client =
		seat->active_client_while_inhibited;
	return inhibiting_client
		&& inhibiting_client != wl_resource_get_client(resource);
}

static bool
update_pressed_surface(struct seat *seat, struct cursor_context *ctx)
{
	/*
	 * In most cases, we don't want to leave one surface and enter
	 * another while a button is pressed.  However, GTK/Wayland
	 * menus (implemented as XDG popups) do need the leave/enter
	 * events to work properly.  To cover this case, we allow
	 * leave/enter events between XDG popups and their toplevel.
	 */
	if (seat->pressed.surface && ctx->surface != seat->pressed.surface) {
		struct wlr_surface *toplevel = get_toplevel(ctx->surface);
		if (toplevel && toplevel == seat->pressed.toplevel) {
			/* No need to recompute resize edges here */
			seat_set_pressed(seat, ctx->view,
				ctx->node, ctx->surface, toplevel,
				seat->pressed.resize_edges);
			return true;
		}
	}
	return false;
}

static void
process_cursor_motion_out_of_surface(struct server *server, uint32_t time)
{
	struct view *view = server->seat.pressed.view;
	struct wlr_scene_node *node = server->seat.pressed.node;
	struct wlr_surface *surface = server->seat.pressed.surface;
	assert(surface);
	int lx, ly;

	if (view) {
		lx = view->x;
		ly = view->y;
	} else if (node && wlr_surface_is_layer_surface(surface)) {
		wlr_scene_node_coords(node, &lx, &ly);
#if HAVE_XWAYLAND
	} else if (node && node->parent == server->unmanaged_tree) {
		wlr_scene_node_coords(node, &lx, &ly);
#endif
	} else {
		wlr_log(WLR_ERROR, "Can't detect surface for out-of-surface movement");
		return;
	}

	double sx = server->seat.cursor->x - lx;
	double sy = server->seat.cursor->y - ly;
	/* Take into account invisible xdg-shell CSD borders */
	if (view && view->type == LAB_XDG_SHELL_VIEW && view->xdg_surface) {
		struct wlr_box geo;
		wlr_xdg_surface_get_geometry(view->xdg_surface, &geo);
		sx += geo.x;
		sy += geo.y;
	}

	wlr_seat_pointer_notify_motion(server->seat.seat, time, sx, sy);
}

/*
 * Common logic shared by cursor_update_focus() and process_cursor_motion()
 */
static void
cursor_update_common(struct server *server, struct cursor_context *ctx,
		uint32_t time_msec, bool cursor_has_moved)
{
	struct seat *seat = &server->seat;
	struct wlr_seat *wlr_seat = seat->seat;

	ssd_update_button_hover(ctx->node, &server->ssd_hover_state);

	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		/*
		 * Prevent updating focus/cursor image during
		 * interactive move/resize
		 */
		return;
	}

	/* TODO: verify drag_icon logic */
	if (seat->pressed.surface && ctx->surface != seat->pressed.surface
			&& !update_pressed_surface(seat, ctx)
			&& !seat->drag_icon) {
		if (cursor_has_moved) {
			/*
			 * Button has been pressed while over another
			 * surface and is still held down.  Just send
			 * the motion events to the focused surface so
			 * we can keep scrolling or selecting text even
			 * if the cursor moves outside of the surface.
			 */
			process_cursor_motion_out_of_surface(server, time_msec);
		}
		return;
	}

	if (ctx->surface && !input_inhibit_blocks_surface(seat,
			ctx->surface->resource)) {
		/*
		 * Cursor is over an input-enabled client surface.  The
		 * cursor image will be set by request_cursor_notify()
		 * in response to the enter event.
		 */
		bool has_focus = (ctx->surface ==
			wlr_seat->pointer_state.focused_surface);

		if (!has_focus || seat->server_cursor != LAB_CURSOR_CLIENT) {
			/*
			 * Enter the surface if necessary.  Usually we
			 * prevent re-entering an already focused
			 * surface, because the extra leave and enter
			 * events can confuse clients (e.g. break
			 * double-click detection).
			 *
			 * We do however send a leave/enter event pair
			 * if a server-side cursor was set and we need
			 * to trigger a cursor image update.
			 */
			if (has_focus) {
				wlr_seat_pointer_notify_clear_focus(wlr_seat);
			}
			wlr_seat_pointer_notify_enter(wlr_seat, ctx->surface,
				ctx->sx, ctx->sy);
			seat->server_cursor = LAB_CURSOR_CLIENT;
		}
		if (cursor_has_moved) {
			wlr_seat_pointer_notify_motion(wlr_seat, time_msec,
				ctx->sx, ctx->sy);
		}
	} else {
		/*
		 * Cursor is over a server (labwc) surface.  Clear focus
		 * from the focused client (if any, no-op otherwise) and
		 * set the cursor image ourselves when not currently in
		 * a drag operation.
		 */
		wlr_seat_pointer_notify_clear_focus(wlr_seat);
		if (!seat->drag_icon) {
			cursor_set(seat, cursor_get_from_ssd(ctx->type));
		}
	}
}

uint32_t
cursor_get_resize_edges(struct wlr_cursor *cursor, struct cursor_context *ctx)
{
	uint32_t resize_edges = ssd_resize_edges(ctx->type);
	if (ctx->view && !resize_edges) {
		resize_edges |=
			(int)cursor->x < ctx->view->x + ctx->view->w / 2 ?
				WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
		resize_edges |=
			(int)cursor->y < ctx->view->y + ctx->view->h / 2 ?
				WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
	}
	return resize_edges;
}

static void
process_cursor_motion(struct server *server, uint32_t time)
{
	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->input_mode == LAB_INPUT_STATE_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->input_mode == LAB_INPUT_STATE_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* Otherwise, find view under the pointer and send the event along */
	struct cursor_context ctx = get_cursor_context(server);
	struct seat *seat = &server->seat;

	if (ctx.type == LAB_SSD_MENU) {
		menu_process_cursor_motion(ctx.node);
		cursor_set(&server->seat, LAB_CURSOR_DEFAULT);
		return;
	}

	if (ctx.view && rc.focus_follow_mouse) {
		desktop_focus_and_activate_view(seat, ctx.view);
		if (rc.raise_on_focus) {
			desktop_move_to_front(ctx.view);
		}
	}

	struct mousebind *mousebind;
	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (mousebind->mouse_event == MOUSE_ACTION_DRAG
				&& mousebind->pressed_in_context) {
			/*
			 * Use view and resize edges from the press
			 * event (not the motion event) to prevent
			 * moving/resizing the wrong view
			 */
			mousebind->pressed_in_context = false;
			actions_run(seat->pressed.view,
				server, &mousebind->actions,
				seat->pressed.resize_edges);
		}
	}

	cursor_update_common(server, &ctx, time, /*cursor_has_moved*/ true);
}

static uint32_t
msec(const struct timespec *t)
{
	return t->tv_sec * 1000 + t->tv_nsec / 1000000;
}

void
cursor_update_focus(struct server *server)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* Focus surface under cursor if it isn't already focused */
	struct cursor_context ctx = get_cursor_context(server);
	cursor_update_common(server, &ctx, msec(&now),
		/*cursor_has_moved*/ false);
}

void
start_drag(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, start_drag);
	struct wlr_drag *wlr_drag = data;
	seat_reset_pressed(seat);
	seat->drag_icon = wlr_drag->icon;
	if (!seat->drag_icon) {
		wlr_log(WLR_ERROR,
			"Started drag but application did not set a drag icon");
		return;
	}
	wl_signal_add(&seat->drag_icon->events.destroy, &seat->destroy_drag);
}

void
handle_constraint_commit(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, constraint_commit);
	struct wlr_pointer_constraint_v1 *constraint = seat->current_constraint;
	assert(constraint->surface = data);
}

void
destroy_constraint(struct wl_listener *listener, void *data)
{
	struct constraint *constraint = wl_container_of(listener, constraint,
		destroy);
	struct wlr_pointer_constraint_v1 *wlr_constraint = data;
	struct seat *seat = constraint->seat;

	wl_list_remove(&constraint->destroy.link);
	if (seat->current_constraint == wlr_constraint) {
		if (seat->constraint_commit.link.next) {
			wl_list_remove(&seat->constraint_commit.link);
		}
		wl_list_init(&seat->constraint_commit.link);
		seat->current_constraint = NULL;
	}

	free(constraint);
}

void
create_constraint(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_constraint_v1 *wlr_constraint = data;
	struct server *server = wl_container_of(listener, server,
		new_constraint);
	struct constraint *constraint = znew(*constraint);

	constraint->constraint = wlr_constraint;
	constraint->seat = &server->seat;
	constraint->destroy.notify = destroy_constraint;
	wl_signal_add(&wlr_constraint->events.destroy, &constraint->destroy);

	struct view *view = desktop_focused_view(server);
	if (view && view->surface == wlr_constraint->surface) {
		constrain_cursor(server, wlr_constraint);
	}
}

void
constrain_cursor(struct server *server, struct wlr_pointer_constraint_v1
		*constraint)
{
	struct seat *seat = &server->seat;
	if (seat->current_constraint == constraint) {
		return;
	}
	wl_list_remove(&seat->constraint_commit.link);
	if (seat->current_constraint) {
		wlr_pointer_constraint_v1_send_deactivated(
			seat->current_constraint);
	}

	seat->current_constraint = constraint;

	if (!constraint) {
		wl_list_init(&seat->constraint_commit.link);
		return;
	}

	wlr_pointer_constraint_v1_send_activated(constraint);
	seat->constraint_commit.notify = handle_constraint_commit;
	wl_signal_add(&constraint->surface->events.commit,
		&seat->constraint_commit);
}

static void
cursor_motion(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta)
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_motion);
	struct server *server = seat->server;
	struct wlr_pointer_motion_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	wlr_relative_pointer_manager_v1_send_relative_motion(
		server->relative_pointer_manager,
		seat->seat, (uint64_t)event->time_msec * 1000,
		event->delta_x, event->delta_y, event->unaccel_dx,
		event->unaccel_dy);
	if (!seat->current_constraint) {
		/*
		 * The cursor doesn't move unless we tell it to. The cursor
		 * automatically handles constraining the motion to the output
		 * layout, as well as any special configuration applied for the
		 * specific input device which generated the event. You can pass
		 * NULL for the device if you want to move the cursor around
		 * without any input.
		 */
		wlr_cursor_move(seat->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
	}
	process_cursor_motion(seat->server, event->time_msec);
}

void
destroy_drag(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, destroy_drag);

	if (!seat->drag_icon) {
		return;
	}
	seat->drag_icon = NULL;
	desktop_focus_topmost_mapped_view(seat->server);
}

void
cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an
	 * _absolute_ motion event, from 0..1 on each axis. This happens, for
	 * example, when wlroots is running under a Wayland window rather than
	 * KMS+DRM, and you move the mouse over the window. You could enter the
	 * window from any edge, so we have to warp the mouse there. There is
	 * also some hardware which emits these events.
	 */
	struct seat *seat = wl_container_of(
		listener, seat, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor,
		&event->pointer->base, event->x, event->y, &lx, &ly);

	double dx = lx - seat->cursor->x;
	double dy = ly - seat->cursor->y;

	wlr_relative_pointer_manager_v1_send_relative_motion(
		seat->server->relative_pointer_manager,
		seat->seat, (uint64_t)event->time_msec * 1000,
		dx, dy, dx, dy);

	if (!seat->current_constraint) {
		/*
		 * The cursor doesn't move unless we tell it to. The cursor
		 * automatically handles constraining the motion to the output
		 * layout, as well as any special configuration applied for the
		 * specific input device which generated the event. You can pass
		 * NULL for the device if you want to move the cursor around
		 * without any input.
		 */
		wlr_cursor_move(seat->cursor, &event->pointer->base, dx, dy);
	}

	process_cursor_motion(seat->server, event->time_msec);
}

static bool
handle_release_mousebinding(struct server *server,
		struct cursor_context *ctx, uint32_t button)
{
	struct mousebind *mousebind;
	bool activated_any = false;
	bool activated_any_frame = false;

	uint32_t modifiers = wlr_keyboard_get_modifiers(
			&server->seat.keyboard_group->keyboard);

	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (ssd_part_contains(mousebind->context, ctx->type)
				&& mousebind->button == button
				&& modifiers == mousebind->modifiers) {
			switch (mousebind->mouse_event) {
			case MOUSE_ACTION_RELEASE:
				break;
			case MOUSE_ACTION_CLICK:
				if (mousebind->pressed_in_context) {
					break;
				}
				continue;
			case MOUSE_ACTION_DRAG:
				if (mousebind->pressed_in_context) {
					/*
					 * Swallow the release event as well as
					 * the press one
					 */
					activated_any = true;
					activated_any_frame |=
						mousebind->context == LAB_SSD_FRAME;
				}
				continue;
			default:
				continue;
			}
			activated_any = true;
			activated_any_frame |= mousebind->context == LAB_SSD_FRAME;
			actions_run(ctx->view, server, &mousebind->actions,
				/*resize_edges*/ 0);
		}
	}
	/*
	 * Clear "pressed" status for all bindings of this mouse button,
	 * regardless of whether activated or not
	 */
	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (mousebind->button == button) {
			mousebind->pressed_in_context = false;
		}
	}
	return activated_any && activated_any_frame;
}

static bool
is_double_click(long double_click_speed, uint32_t button, struct view *view)
{
	static uint32_t last_button;
	static struct view *last_view;
	static struct timespec last_click;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	long ms = (now.tv_sec - last_click.tv_sec) * 1000 +
		(now.tv_nsec - last_click.tv_nsec) / 1000000;
	last_click = now;
	if (last_button != button || last_view != view) {
		last_button = button;
		last_view = view;
		return false;
	}
	if (ms < double_click_speed && ms >= 0) {
		/*
		 * End sequence so that third click is not considered a
		 * double-click
		 */
		last_button = 0;
		last_view = NULL;
		return true;
	}
	return false;
}

static bool
handle_press_mousebinding(struct server *server, struct cursor_context *ctx,
		uint32_t button, uint32_t resize_edges)
{
	struct mousebind *mousebind;
	bool double_click = is_double_click(rc.doubleclick_time, button, ctx->view);
	bool activated_any = false;
	bool activated_any_frame = false;

	uint32_t modifiers = wlr_keyboard_get_modifiers(
			&server->seat.keyboard_group->keyboard);

	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		if (ssd_part_contains(mousebind->context, ctx->type)
				&& mousebind->button == button
				&& modifiers == mousebind->modifiers) {
			switch (mousebind->mouse_event) {
			case MOUSE_ACTION_DRAG: /* fallthrough */
			case MOUSE_ACTION_CLICK:
				/*
				 * DRAG and CLICK actions will be processed on
				 * the release event, unless the press event is
				 * counted as a DOUBLECLICK.
				 */
				if (!double_click) {
					/*
					 * Swallow the press event as well as
					 * the release one
					 */
					activated_any = true;
					activated_any_frame |=
						mousebind->context == LAB_SSD_FRAME;
					mousebind->pressed_in_context = true;
				}
				continue;
			case MOUSE_ACTION_DOUBLECLICK:
				if (!double_click) {
					continue;
				}
				break;
			case MOUSE_ACTION_PRESS:
				break;
			default:
				continue;
			}
			activated_any = true;
			activated_any_frame |= mousebind->context == LAB_SSD_FRAME;
			actions_run(ctx->view, server, &mousebind->actions, resize_edges);
		}
	}
	return activated_any && activated_any_frame;
}

/* Set in cursor_button_press(), used in cursor_button_release() */
static bool close_menu;

static void
cursor_button_press(struct seat *seat, struct wlr_pointer_button_event *event)
{
	struct server *server = seat->server;
	struct cursor_context ctx = get_cursor_context(server);

	/* Determine closest resize edges in case action is Resize */
	uint32_t resize_edges = cursor_get_resize_edges(seat->cursor, &ctx);

	if (ctx.view || ctx.surface) {
		/* Store resize edges for later action processing */
		seat_set_pressed(seat, ctx.view, ctx.node, ctx.surface,
			get_toplevel(ctx.surface), resize_edges);
	}

	if (server->input_mode == LAB_INPUT_STATE_MENU) {
		/* We are closing the menu on RELEASE to not leak a stray release */
		if (ctx.type != LAB_SSD_MENU) {
			close_menu = true;
		} else if (menu_call_actions(ctx.node)) {
			/* Action was successfull, may fail if item just opens a submenu */
			close_menu = true;
		}
		return;
	}

	/* Handle _press_ on a layer surface */
	if (ctx.type == LAB_SSD_LAYER_SURFACE) {
		struct wlr_layer_surface_v1 *layer =
			wlr_layer_surface_v1_from_wlr_surface(ctx.surface);
		if (layer->current.keyboard_interactive) {
			seat_set_focus_layer(seat, layer);
		}
	}

	/* Bindings to the Frame context swallow mouse events if activated */
	bool triggered_frame_binding = handle_press_mousebinding(
			server, &ctx, event->button, resize_edges);

	if (ctx.surface && !triggered_frame_binding) {
		/* Notify client with pointer focus of button press */
		wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
			event->button, event->state);
	}
}

static void
cursor_button_release(struct seat *seat, struct wlr_pointer_button_event *event)
{
	struct server *server = seat->server;
	struct cursor_context ctx = get_cursor_context(server);
	struct wlr_surface *pressed_surface = seat->pressed.surface;

	seat_reset_pressed(seat);

	if (pressed_surface && ctx.surface != pressed_surface) {
		/*
		 * Button released but originally pressed over a different surface.
		 * Just send the release event to the still focused surface.
		 */
		wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
			event->button, event->state);
		return;
	}

	if (server->input_mode == LAB_INPUT_STATE_MENU) {
		if (close_menu) {
			menu_close_root(server);
			cursor_update_common(server, &ctx, event->time_msec,
				/*cursor_has_moved*/ false);
			close_menu = false;
		}
		return;
	}

	if (server->input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		/* Exit interactive move/resize mode */
		interactive_end(server->grabbed_view);
		return;
	}

	/* Bindings to the Frame context swallow mouse events if activated */
	bool triggered_frame_binding =
		handle_release_mousebinding(server, &ctx, event->button);

	if (ctx.surface && !triggered_frame_binding) {
		/* Notify client with pointer focus of button release */
		wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
			event->button, event->state);
	}
}

void
cursor_button(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits a button
	 * event.
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_button);
	struct wlr_pointer_button_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	switch (event->state) {
	case WLR_BUTTON_PRESSED:
		cursor_button_press(seat, event);
		break;
	case WLR_BUTTON_RELEASED:
		cursor_button_release(seat, event);
		break;
	}
}

void
cursor_axis(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an axis
	 * event, for example when you move the scroll wheel.
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	wlr_idle_notify_activity(seat->wlr_idle, seat->seat);

	/* Make sure we are sending the events to the surface under the cursor */
	cursor_update_focus(seat->server);

	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat->seat, event->time_msec,
		event->orientation, event->delta, event->delta_discrete,
		event->source);
}

void
cursor_frame(struct wl_listener *listener, void *data)
{
	/*
	 * This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen
	 * at the same time, in which case a frame event won't be sent in
	 * between.
	 */
	struct seat *seat = wl_container_of(listener, seat, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat->seat);
}

static void handle_pointer_pinch_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_begin);
	struct wlr_pointer_pinch_begin_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void handle_pointer_pinch_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_update);
	struct wlr_pointer_pinch_update_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

static void handle_pointer_pinch_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, pinch_end);
	struct wlr_pointer_pinch_end_event *event = data;
	wlr_pointer_gestures_v1_send_pinch_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

static void handle_pointer_swipe_begin(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_begin);
	struct wlr_pointer_swipe_begin_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_begin(seat->pointer_gestures,
		seat->seat, event->time_msec, event->fingers);
}

static void handle_pointer_swipe_update(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_update);
	struct wlr_pointer_swipe_update_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_update(seat->pointer_gestures,
		seat->seat, event->time_msec, event->dx, event->dy);
}

static void handle_pointer_swipe_end(struct wl_listener *listener, void *data)
{
	struct seat *seat = wl_container_of(listener, seat, swipe_end);
	struct wlr_pointer_swipe_end_event *event = data;
	wlr_pointer_gestures_v1_send_swipe_end(seat->pointer_gestures,
		seat->seat, event->time_msec, event->cancelled);
}

void
cursor_init(struct seat *seat)
{
	const char *xcursor_theme = getenv("XCURSOR_THEME");
	const char *xcursor_size = getenv("XCURSOR_SIZE");
	uint32_t size = xcursor_size ? atoi(xcursor_size) : 24;

	seat->xcursor_manager = wlr_xcursor_manager_create(xcursor_theme, size);
	wlr_xcursor_manager_load(seat->xcursor_manager, 1);

	if (wlr_xcursor_manager_get_xcursor(seat->xcursor_manager,
			cursors_xdg[LAB_CURSOR_DEFAULT], 1)) {
		cursor_names = cursors_xdg;
	} else {
		wlr_log(WLR_INFO,
			"Cursor theme is missing cursor names, using fallback");
		cursor_names = cursors_x11;
	}

	seat->cursor_motion.notify = cursor_motion;
	wl_signal_add(&seat->cursor->events.motion, &seat->cursor_motion);
	seat->cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&seat->cursor->events.motion_absolute,
		      &seat->cursor_motion_absolute);
	seat->cursor_button.notify = cursor_button;
	wl_signal_add(&seat->cursor->events.button, &seat->cursor_button);
	seat->cursor_axis.notify = cursor_axis;
	wl_signal_add(&seat->cursor->events.axis, &seat->cursor_axis);
	seat->cursor_frame.notify = cursor_frame;
	wl_signal_add(&seat->cursor->events.frame, &seat->cursor_frame);

	seat->pointer_gestures = wlr_pointer_gestures_v1_create(seat->server->wl_display);
	seat->pinch_begin.notify = handle_pointer_pinch_begin;
	wl_signal_add(&seat->cursor->events.pinch_begin, &seat->pinch_begin);
	seat->pinch_update.notify = handle_pointer_pinch_update;
	wl_signal_add(&seat->cursor->events.pinch_update, &seat->pinch_update);
	seat->pinch_end.notify = handle_pointer_pinch_end;
	wl_signal_add(&seat->cursor->events.pinch_end, &seat->pinch_end);
	seat->swipe_begin.notify = handle_pointer_swipe_begin;
	wl_signal_add(&seat->cursor->events.swipe_begin, &seat->swipe_begin);
	seat->swipe_update.notify = handle_pointer_swipe_update;
	wl_signal_add(&seat->cursor->events.swipe_update, &seat->swipe_update);
	seat->swipe_end.notify = handle_pointer_swipe_end;
	wl_signal_add(&seat->cursor->events.swipe_end, &seat->swipe_end);

	seat->request_cursor.notify = request_cursor_notify;
	wl_signal_add(&seat->seat->events.request_set_cursor,
		&seat->request_cursor);
	seat->request_set_selection.notify = request_set_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_selection,
		&seat->request_set_selection);
	seat->request_start_drag.notify = request_start_drag_notify;
	wl_signal_add(&seat->seat->events.request_start_drag,
		&seat->request_start_drag);
	seat->start_drag.notify = start_drag;
	wl_signal_add(&seat->seat->events.start_drag,
		&seat->start_drag);
	seat->destroy_drag.notify = destroy_drag;

	seat->request_set_primary_selection.notify =
		request_set_primary_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_primary_selection,
		&seat->request_set_primary_selection);
}

void cursor_finish(struct seat *seat)
{
	wl_list_remove(&seat->cursor_motion.link);
	wl_list_remove(&seat->cursor_motion_absolute.link);
	wl_list_remove(&seat->cursor_button.link);
	wl_list_remove(&seat->cursor_axis.link);
	wl_list_remove(&seat->cursor_frame.link);

	wl_list_remove(&seat->pinch_begin.link);
	wl_list_remove(&seat->pinch_update.link);
	wl_list_remove(&seat->pinch_end.link);
	wl_list_remove(&seat->swipe_begin.link);
	wl_list_remove(&seat->swipe_update.link);
	wl_list_remove(&seat->swipe_end.link);

	wl_list_remove(&seat->request_cursor.link);
	wl_list_remove(&seat->request_set_selection.link);

	wlr_xcursor_manager_destroy(seat->xcursor_manager);
	wlr_cursor_destroy(seat->cursor);
}
