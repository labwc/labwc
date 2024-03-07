/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SURFACE_HELPERS_H
#define LABWC_SURFACE_HELPERS_H

struct wlr_surface;
struct wlr_layer_surface_v1;

/**
 * subsurface_parent_layer() - Get wlr_layer_surface from layer-subsurface
 * @wlr_surface: The wlr_surface of the wlr_subsurface for which to get the
 *               layer-surface.
 */
struct wlr_layer_surface_v1 *subsurface_parent_layer(
	struct wlr_surface *wlr_surface);

#endif /* LABWC_SURFACE_HELPERS_H */
