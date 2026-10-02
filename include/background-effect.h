/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BACKGROUND_EFFECT_H
#define LABWC_BACKGROUND_EFFECT_H

#include <pixman.h>
#include <stdint.h>
#include <wayland-server-core.h>

#include "ext-background-effect-v1-protocol.h"

struct wlr_surface;

/*
 * Per-surface state of ext-background-effect-v1. The region is in
 * surface-local coordinates and is applied (copied from the pending
 * state) on wl_surface.commit, as described by the protocol.
 */
struct background_effect_surface_state {
	pixman_region32_t blur_region;
};

struct background_effect;

/*
 * Advertise ext_background_effect_manager_v1 on the registry.
 *
 * @capabilities is a bitmask of enum
 * ext_background_effect_manager_v1_capability.
 *
 * Returns NULL on allocation failure.
 */
struct background_effect *background_effect_create(struct wl_display *display,
	uint32_t capabilities);

/*
 * Returns the current (committed) background-effect state of @surface,
 * or NULL if no ext_background_effect_surface_v1 object was ever created
 * for it.
 *
 * The returned pointer is valid until the wl_surface or the
 * ext_background_effect_surface_v1 object is destroyed.
 */
const struct background_effect_surface_state *
background_effect_get_state(struct wlr_surface *surface);

#endif /* LABWC_BACKGROUND_EFFECT_H */
