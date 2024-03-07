// SPDX-License-Identifier: GPL-2.0-only
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>
#include "common/surface-helpers.h"

struct wlr_layer_surface_v1 *
subsurface_parent_layer(struct wlr_surface *wlr_surface)
{
	struct wlr_subsurface *subsurface =
		wlr_subsurface_try_from_wlr_surface(wlr_surface);
	if (!subsurface) {
		wlr_log(WLR_DEBUG, "surface %p is not subsurface", subsurface);
		return NULL;
	}
	struct wlr_surface *parent = subsurface->parent;
	if (!parent) {
		wlr_log(WLR_DEBUG, "subsurface %p has no parent", subsurface);
		return NULL;
	}
	struct wlr_layer_surface_v1 *wlr_layer_surface =
		wlr_layer_surface_v1_try_from_wlr_surface(parent);
	if (wlr_layer_surface) {
		return wlr_layer_surface;
	}
	/* Recurse in case there are nested sub-surfaces */
	return subsurface_parent_layer(parent);
}
