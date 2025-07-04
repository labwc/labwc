/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_FOREIGN_TOPLEVEL_INTERNAL_H
#define LABWC_FOREIGN_TOPLEVEL_INTERNAL_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include "foreign-toplevel.h"

struct foreign_toplevel {
	struct view *view;

	/* *-toplevel implementations */
	struct wlr_foreign_toplevel {
		struct wlr_foreign_toplevel_handle_v1 *handle;

		/* Client side events */
		struct {
			struct wl_listener request_maximize;
			struct wl_listener request_minimize;
			struct wl_listener request_fullscreen;
			struct wl_listener request_activate;
			struct wl_listener request_close;
			struct wl_listener handle_destroy;
		} on;

		/* Compositor side state updates */
		struct {
			struct wl_listener new_app_id;
			struct wl_listener new_title;
			struct wl_listener new_outputs;
			struct wl_listener maximized;
			struct wl_listener minimized;
			struct wl_listener fullscreened;
			struct wl_listener activated;
		} on_view;

		/* Internal signals */
		struct {
			struct wl_listener toplevel_parent;
			struct wl_listener toplevel_destroy;
		} on_foreign_toplevel;

	} wlr_toplevel;

	struct ext_foreign_toplevel {
		struct wlr_ext_foreign_toplevel_handle_v1 *handle;

		/* Client side events */
		struct {
			struct wl_listener handle_destroy;
		} on;

		/* Compositor side state updates */
		struct {
			struct wl_listener new_app_id;
			struct wl_listener new_title;
		} on_view;

		/* Internal signals */
		struct {
			struct wl_listener toplevel_destroy;
		} on_foreign_toplevel;

	} ext_toplevel;

	/* TODO: add struct xdg_x11_mapped_toplevel at some point */

	struct {
		struct wl_signal toplevel_parent;  /* struct view *parent */
		struct wl_signal toplevel_destroy;
	} events;
};

void ext_foreign_toplevel_init(struct foreign_toplevel *toplevel);
void wlr_foreign_toplevel_init(struct foreign_toplevel *toplevel);

void foreign_request_minimize(struct foreign_toplevel *toplevel, bool minimized);
void foreign_request_maximize(struct foreign_toplevel *toplevel, enum view_axis axis);
void foreign_request_fullscreen(struct foreign_toplevel *toplevel, bool fullscreen);
void foreign_request_activate(struct foreign_toplevel *toplevel);
void foreign_request_close(struct foreign_toplevel *toplevel);

#endif /* LABWC_FOREIGN_TOPLEVEL_INTERNAL_H */
