/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_EXT_FOREIGN_TOPLEVEL_H
#define LABWC_EXT_FOREIGN_TOPLEVEL_H

#include <wayland-server-core.h>

struct ext_foreign_toplevel {
	struct view *view;
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
};

void ext_foreign_toplevel_init(struct ext_foreign_toplevel *ext_toplevel,
	struct view *view);
void ext_foreign_toplevel_finish(struct ext_foreign_toplevel *ext_toplevel);

#endif /* LABWC_EXT_FOREIGN_TOPLEVEL_H */
