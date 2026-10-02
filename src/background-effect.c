// SPDX-License-Identifier: GPL-2.0-only
/*
 * background-effect.c - ext-background-effect-v1 support for labwc
 *
 * Protocol implementation based on the ext-background-effect-v1 code in
 * wlroots (MIT licensed), see wlroots/types/wlr_ext_background_effect_v1.c
 * (gitlab.freedesktop.org/wlroots/wlroots !5304). labwc cannot use it
 * because the protocol was merged into wlroots master (0.21) while labwc
 * links against wlroots-0.20, so the ~250 lines of protocol handling are
 * implemented here instead.
 *
 * Only the protocol state is tracked for now; actually rendering the blur
 * behind a surface needs a scene-graph renderer with blur support
 * (e.g. SceneFX) and is tracked in labwc/labwc discussion #3391.
 */
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/addon.h>
#include <wlr/util/log.h>

#include "background-effect.h"
#include "ext-background-effect-v1-protocol.h"

#define BACKGROUND_EFFECT_VERSION 1

struct background_effect_surface {
	struct wl_resource *resource;
	struct wlr_surface *wlr_surface;
	struct wlr_addon addon;
	struct wlr_surface_synced synced;
	struct background_effect_surface_state pending, current;
};

struct background_effect {
	struct wl_global *global;
	struct wl_list resources; /* wl_resource.link */
	uint32_t capabilities;
	struct wl_listener display_destroy;
};

static const struct ext_background_effect_surface_v1_interface surface_impl;
static const struct ext_background_effect_manager_v1_interface manager_impl;

static struct background_effect_surface *
surface_from_resource(struct wl_resource *resource)
{
	assert(wl_resource_instance_of(resource,
		&ext_background_effect_surface_v1_interface, &surface_impl));
	return wl_resource_get_user_data(resource);
}

static void
background_effect_surface_destroy(struct background_effect_surface *surface)
{
	if (!surface) {
		return;
	}
	wlr_surface_synced_finish(&surface->synced);
	wlr_addon_finish(&surface->addon);
	wl_resource_set_user_data(surface->resource, NULL);
	free(surface);
}

static void
surface_handle_resource_destroy(struct wl_resource *resource)
{
	background_effect_surface_destroy(surface_from_resource(resource));
}

static void
surface_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
surface_handle_set_blur_region(struct wl_client *client,
		struct wl_resource *resource,
		struct wl_resource *region_resource)
{
	struct background_effect_surface *surface =
		surface_from_resource(resource);

	if (!surface) {
		/* The associated wl_surface has been destroyed: inert object */
		wl_resource_post_error(resource,
			EXT_BACKGROUND_EFFECT_SURFACE_V1_ERROR_SURFACE_DESTROYED,
			"The wl_surface object has been destroyed");
		return;
	}

	if (region_resource) {
		const pixman_region32_t *region =
			wlr_region_from_resource(region_resource);
		pixman_region32_copy(&surface->pending.blur_region, region);
	} else {
		pixman_region32_clear(&surface->pending.blur_region);
	}
}

static const struct ext_background_effect_surface_v1_interface surface_impl = {
	.destroy = surface_handle_destroy,
	.set_blur_region = surface_handle_set_blur_region,
};

static void
surface_synced_init_state(void *_state)
{
	struct background_effect_surface_state *state = _state;
	pixman_region32_init(&state->blur_region);
}

static void
surface_synced_finish_state(void *_state)
{
	struct background_effect_surface_state *state = _state;
	pixman_region32_fini(&state->blur_region);
}

static void
surface_synced_move_state(void *_dst, void *_src)
{
	struct background_effect_surface_state *dst = _dst;
	struct background_effect_surface_state *src = _src;
	pixman_region32_copy(&dst->blur_region, &src->blur_region);
}

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct background_effect_surface_state),
	.init_state = surface_synced_init_state,
	.finish_state = surface_synced_finish_state,
	.move_state = surface_synced_move_state,
};

static void
surface_addon_destroy(struct wlr_addon *addon)
{
	struct background_effect_surface *surface =
		wl_container_of(addon, surface, addon);
	background_effect_surface_destroy(surface);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "background_effect_surface",
	.destroy = surface_addon_destroy,
};

static struct background_effect_surface *
surface_from_wlr_surface(struct wlr_surface *wlr_surface)
{
	struct wlr_addon *addon = wlr_addon_find(&wlr_surface->addons,
		NULL, &surface_addon_impl);
	if (!addon) {
		return NULL;
	}
	struct background_effect_surface *surface =
		wl_container_of(addon, surface, addon);
	return surface;
}

const struct background_effect_surface_state *
background_effect_get_state(struct wlr_surface *wlr_surface)
{
	struct background_effect_surface *surface =
		surface_from_wlr_surface(wlr_surface);
	if (!surface) {
		return NULL;
	}
	return &surface->current;
}

static void
manager_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
manager_handle_get_background_effect(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *surface_resource)
{
	struct wlr_surface *wlr_surface =
		wlr_surface_from_resource(surface_resource);

	if (surface_from_wlr_surface(wlr_surface)) {
		wl_resource_post_error(manager_resource,
			EXT_BACKGROUND_EFFECT_MANAGER_V1_ERROR_BACKGROUND_EFFECT_EXISTS,
			"The wl_surface object already has a "
			"ext_background_effect_surface_v1 object");
		return;
	}

	struct background_effect_surface *surface =
		calloc(1, sizeof(*surface));
	if (!surface) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	if (!wlr_surface_synced_init(&surface->synced, wlr_surface,
			&surface_synced_impl, &surface->pending,
			&surface->current)) {
		free(surface);
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	uint32_t version = wl_resource_get_version(manager_resource);
	surface->resource = wl_resource_create(client,
		&ext_background_effect_surface_v1_interface, version, id);
	if (!surface->resource) {
		wlr_surface_synced_finish(&surface->synced);
		free(surface);
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	wl_resource_set_implementation(surface->resource, &surface_impl,
		surface, surface_handle_resource_destroy);

	surface->wlr_surface = wlr_surface;
	wlr_addon_init(&surface->addon, &wlr_surface->addons, NULL,
		&surface_addon_impl);

	wlr_log(WLR_DEBUG, "ext-background-effect-v1: new background effect "
		"object for surface %p", (void *)wlr_surface);
}

static const struct ext_background_effect_manager_v1_interface manager_impl = {
	.destroy = manager_handle_destroy,
	.get_background_effect = manager_handle_get_background_effect,
};

static void
manager_handle_resource_destroy(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static void
manager_bind(struct wl_client *wl_client, void *data, uint32_t version,
		uint32_t id)
{
	struct background_effect *manager = data;

	struct wl_resource *resource = wl_resource_create(wl_client,
		&ext_background_effect_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(wl_client);
		return;
	}

	wl_resource_set_implementation(resource, &manager_impl, manager,
		manager_handle_resource_destroy);
	wl_list_insert(&manager->resources, wl_resource_get_link(resource));

	ext_background_effect_manager_v1_send_capabilities(resource,
		manager->capabilities);
}

static void
handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct background_effect *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_global_destroy(manager->global);
	wl_list_remove(&manager->display_destroy.link);
	free(manager);
}

struct background_effect *
background_effect_create(struct wl_display *display, uint32_t capabilities)
{
	struct background_effect *manager = calloc(1, sizeof(*manager));
	if (!manager) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&ext_background_effect_manager_v1_interface,
		BACKGROUND_EFFECT_VERSION, manager, manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}

	manager->capabilities = capabilities;
	wl_list_init(&manager->resources);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	wlr_log(WLR_INFO, "ext-background-effect-v1: advertised with "
		"capabilities 0x%x", capabilities);
	return manager;
}
