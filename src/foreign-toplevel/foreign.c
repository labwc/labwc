// SPDX-License-Identifier: GPL-2.0-only
#include "foreign-toplevel/foreign.h"
#include <assert.h>
#include "common/mem.h"
#include "foreign-toplevel/ext-foreign.h"
#include "foreign-toplevel/wlr-foreign.h"
#include "view.h"

struct foreign_toplevel {
	/* *-toplevel implementations */
	struct wlr_foreign_toplevel wlr_toplevel;
	struct ext_foreign_toplevel ext_toplevel;

	/* TODO: add struct xdg_x11_mapped_toplevel at some point */
};

struct foreign_toplevel *
foreign_toplevel_create(struct view *view)
{
	assert(view);

	struct foreign_toplevel *toplevel = znew(*toplevel);
	wlr_foreign_toplevel_init(&toplevel->wlr_toplevel, view);
	ext_foreign_toplevel_init(&toplevel->ext_toplevel, view);

	return toplevel;
}

void
foreign_toplevel_set_parent(struct foreign_toplevel *toplevel, struct foreign_toplevel *parent)
{
	assert(toplevel);
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
