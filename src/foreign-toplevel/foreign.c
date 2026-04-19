// SPDX-License-Identifier: GPL-2.0-only
#include "foreign-toplevel/foreign.h"
#include <assert.h>
#include <stdbool.h>
#include "common/mem.h"
#include "foreign-toplevel/ext-foreign.h"
#include "foreign-toplevel/wlr-foreign.h"
#include "labwc.h"
#include "view.h"

struct foreign_toplevel {
	struct view *view;

	/* *-toplevel implementations */
	struct wlr_foreign_toplevel wlr_toplevel;
	struct ext_foreign_toplevel ext_toplevel;
	/* TODO: add struct xdg_x11_mapped_toplevel at some point */
};

static bool
should_export_to_panels(struct foreign_toplevel *toplevel)
{
	struct view *view = toplevel->view;

	if (!view || !view->mapped) {
		return false;
	}

	if (view->visible_on_all_workspaces) {
		return true;
	}

	return view->workspace == server.workspaces.current;
}

static bool
is_exported(struct foreign_toplevel *toplevel)
{
	return !!toplevel->wlr_toplevel.handle || !!toplevel->ext_toplevel.handle;
}

void
foreign_toplevel_refresh(struct foreign_toplevel *toplevel)
{
	bool want, have;

	assert(toplevel);

	want = should_export_to_panels(toplevel);
	have = is_exported(toplevel);

	if (want == have) {
		return;
	}

	if (want) {
		wlr_foreign_toplevel_init(&toplevel->wlr_toplevel, toplevel->view);
		ext_foreign_toplevel_init(&toplevel->ext_toplevel, toplevel->view);
	} else {
		wlr_foreign_toplevel_finish(&toplevel->wlr_toplevel);
		ext_foreign_toplevel_finish(&toplevel->ext_toplevel);
	}
}

struct foreign_toplevel *
foreign_toplevel_create(struct view *view)
{
	assert(view);

	struct foreign_toplevel *toplevel = znew(*toplevel);
	toplevel->view = view;
	foreign_toplevel_refresh(toplevel);

	return toplevel;
}

void
foreign_toplevel_set_parent(struct foreign_toplevel *toplevel, struct foreign_toplevel *parent)
{
	assert(toplevel);

	if (!toplevel->wlr_toplevel.handle) {
		return;
	}

	wlr_foreign_toplevel_set_parent(&toplevel->wlr_toplevel,
		parent ? &parent->wlr_toplevel : NULL);
}

void
foreign_toplevel_destroy(struct foreign_toplevel *toplevel)
{
	assert(toplevel);
	wlr_foreign_toplevel_finish(&toplevel->wlr_toplevel);
	ext_foreign_toplevel_finish(&toplevel->ext_toplevel);
	free(toplevel);
}
