// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wayland-server-core.h>
#include "common/macros.h"
#include "common/mem.h"
#include "labwc.h"
#include "view.h"
#include "foreign-toplevel-internal.h"

/* Internal API */
void
foreign_request_minimize(struct foreign_toplevel *toplevel, bool minimized)
{
	view_minimize(toplevel->view, minimized);
}

void
foreign_request_maximize(struct foreign_toplevel *toplevel, enum view_axis axis)
{
	view_maximize(toplevel->view, axis, /*store_natural_geometry*/ true);
}

void
foreign_request_fullscreen(struct foreign_toplevel *toplevel, bool fullscreen)
{
	view_set_fullscreen(toplevel->view, fullscreen);
}

void
foreign_request_activate(struct foreign_toplevel *toplevel)
{
	if (toplevel->view->server->osd_state.cycle_view) {
		wlr_log(WLR_INFO, "Preventing focus request while in window switcher");
		return;
	}

	desktop_focus_view(toplevel->view, /*raise*/ true);
}

void
foreign_request_close(struct foreign_toplevel *toplevel)
{
	view_close(toplevel->view);
}

/* Public API */
struct foreign_toplevel *
foreign_toplevel_create(struct view *view)
{
	assert(view);
	assert(view->mapped);

	struct foreign_toplevel *toplevel = znew(*toplevel);
	toplevel->view = view;

	wl_signal_init(&toplevel->events.toplevel_parent);
	wl_signal_init(&toplevel->events.toplevel_destroy);

	wlr_foreign_toplevel_init(toplevel);
	ext_foreign_toplevel_init(toplevel);

	return toplevel;
}

void
foreign_toplevel_set_parent(struct foreign_toplevel *toplevel, struct foreign_toplevel *parent)
{
	assert(toplevel);
	wl_signal_emit_mutable(&toplevel->events.toplevel_parent, parent);
}

void
foreign_toplevel_destroy(struct foreign_toplevel *toplevel)
{
	assert(toplevel);
	wl_signal_emit_mutable(&toplevel->events.toplevel_destroy, NULL);
	assert(!toplevel->wlr_toplevel.handle);
	assert(!toplevel->ext_toplevel.handle);
	free(toplevel);
}
