// SPDX-License-Identifier: GPL-2.0-only
/*
 * background-effect.c - ext-background-effect-v1 support for labwc
 *
 * Protocol handling is based on the ext-background-effect-v1 code in
 * wlroots (MIT licensed), see wlroots/types/wlr_ext_background_effect_v1.c
 * (gitlab.freedesktop.org/wlroots/wlroots !5304). labwc cannot use it
 * because the protocol was merged into wlroots master (0.21) while labwc
 * links against wlroots-0.20, so the protocol handling is implemented
 * here instead.
 *
 * The blur itself is rendered by SceneFX: for every surface that requests
 * a blur region, wlr_scene_blur node(s) are kept inside the scene-tree
 * that holds the surface, lowered below the surface content. The nodes
 * blur everything that is rendered below them (wallpaper, other windows)
 * - the same approximation suggested by Consolatis in
 * labwc/labwc discussion #3391.
 *
 * The scene-node rendering a surface is cached (and invalidated when it
 * is destroyed) so that the scene-graph only has to be searched once per
 * surface. Blur parameters come from the <blur> section of rc.xml and are
 * (re)applied by background_effect_reconfigure().
 */
#include <assert.h>
#include <stdlib.h>
#include <scenefx/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/addon.h>
#include <wlr/util/log.h>

#include "background-effect.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "ext-background-effect-v1-protocol.h"
#include "labwc.h"
#include "output.h"

#define BACKGROUND_EFFECT_VERSION 1

/* A single wlr_scene_blur node owned by a background_effect_surface */
struct blur_node {
	struct wl_list link; /* background_effect_surface.blur_nodes */
	struct wlr_scene_blur *blur;
	struct wl_listener destroy;
};

struct background_effect_surface {
	struct wl_resource *resource;
	struct wlr_surface *wlr_surface;
	struct wlr_addon addon;
	struct wlr_surface_synced synced;
	struct background_effect_surface_state pending, current;

	struct wl_list link; /* surfaces */

	/* Scene-graph nodes visualising `current.blur_region` */
	struct wl_list blur_nodes; /* blur_node.link */
	pixman_region32_t applied; /* region the blur_nodes cover */
	struct wlr_scene_tree *applied_tree; /* tree the blur_nodes live in */
	int applied_x, applied_y; /* surface node offset inside applied_tree */

	/*
	 * Scene-node rendering wlr_surface, cached to avoid searching the
	 * whole scene-graph on every commit. Cleared (and the blur nodes
	 * dropped) when the node is destroyed.
	 */
	struct wlr_scene_node *surface_node;
	struct wl_listener surface_node_destroy;
};

struct background_effect {
	struct wl_global *global;
	struct wl_list resources; /* wl_resource.link */
	uint32_t capabilities;
	struct wl_listener display_destroy;
};

/*
 * All live background_effect_surface objects, so that the <blur> config
 * can be re-applied to already existing blur nodes.
 */
static struct wl_list surfaces = { &surfaces, &surfaces };

static const struct ext_background_effect_surface_v1_interface surface_impl;
static const struct ext_background_effect_manager_v1_interface manager_impl;

static struct background_effect_surface *
surface_from_resource(struct wl_resource *resource)
{
	assert(wl_resource_instance_of(resource,
		&ext_background_effect_surface_v1_interface, &surface_impl));
	return wl_resource_get_user_data(resource);
}

/*
 * Find the scene-node directly rendering @surface. Blur nodes are added to
 * the tree containing that node so that they render above everything below
 * the window but below the window content itself.
 *
 * Returns NULL when the surface is not part of the scene-graph (yet),
 * e.g. for views on an invisible workspace.
 */
static struct wlr_scene_node *
find_surface_node(struct wlr_scene_tree *root, struct wlr_surface *surface)
{
	struct wlr_scene_node *child, *tmp;
	wl_list_for_each_safe(child, tmp, &root->children, link) {
		if (child->type == WLR_SCENE_NODE_TREE) {
			struct wlr_scene_node *found = find_surface_node(
				wlr_scene_tree_from_node(child), surface);
			if (found) {
				return found;
			}
		} else if (child->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_surface *scene_surface =
				wlr_scene_surface_try_from_buffer(
					wlr_scene_buffer_from_node(child));
			if (scene_surface && scene_surface->surface == surface) {
				return child;
			}
		}
	}
	return NULL;
}

static void
handle_blur_node_destroy(struct wl_listener *listener, void *data)
{
	struct blur_node *blur_node =
		wl_container_of(listener, blur_node, destroy);
	wl_list_remove(&blur_node->destroy.link);
	wl_list_remove(&blur_node->link);
	free(blur_node);
}

static void
blur_nodes_destroy(struct background_effect_surface *surface)
{
	struct blur_node *blur_node, *tmp;
	if (!wl_list_empty(&surface->blur_nodes)) {
		wlr_log(WLR_DEBUG, "ext-background-effect-v1: destroying %d "
			"blur node(s) for surface %p",
			(int)wl_list_length(&surface->blur_nodes),
			(void *)surface->wlr_surface);
	}
	wl_list_for_each_safe(blur_node, tmp, &surface->blur_nodes, link) {
		/* frees blur_node through handle_blur_node_destroy() */
		wlr_scene_node_destroy(&blur_node->blur->node);
	}
	pixman_region32_clear(&surface->applied);
	surface->applied_tree = NULL;
}

/*
 * The scene-node rendering the surface went away (view unmapped, content
 * re-created, ...): drop the blur nodes living next to it and cache the
 * surface node anew on the next commit.
 */
static void
handle_surface_node_destroy(struct wl_listener *listener, void *data)
{
	struct background_effect_surface *surface =
		wl_container_of(listener, surface, surface_node_destroy);
	wl_list_remove(&surface->surface_node_destroy.link);
	surface->surface_node = NULL;
	/* no-op if the whole scene-tree is being destroyed */
	blur_nodes_destroy(surface);
}

/*
 * Create/destroy wlr_scene_blur nodes so that they match the committed
 * blur region of the surface.
 */
static void
blur_nodes_update(struct background_effect_surface *surface)
{
	struct background_effect_surface_state *state = &surface->current;
	int surface_width = surface->wlr_surface->current.width;
	int surface_height = surface->wlr_surface->current.height;
	pixman_region32_t region;

	/* The region is clipped to the surface, as required by the protocol */
	pixman_region32_init(&region);
	if (pixman_region32_not_empty(&state->blur_region)
			&& surface_width > 0 && surface_height > 0) {
		pixman_region32_t surface_box;
		pixman_region32_init_rect(&surface_box, 0, 0,
			surface_width, surface_height);
		pixman_region32_intersect(&region, &surface_box,
			&state->blur_region);
		pixman_region32_fini(&surface_box);
	}

	if (!pixman_region32_not_empty(&region)) {
		/* No blur: empty region (or an unmapped surface) */
		blur_nodes_destroy(surface);
		pixman_region32_fini(&region);
		return;
	}

	/*
	 * Locate the scene-node rendering the surface. It is cached, so the
	 * scene-graph is only searched once per surface; the cache is
	 * invalidated by handle_surface_node_destroy().
	 */
	struct wlr_scene_node *node = surface->surface_node;
	if (!node && server.scene) {
		node = find_surface_node(&server.scene->tree,
			surface->wlr_surface);
		if (!node) {
			/* Not in the scene-graph; retried on the next commit */
			pixman_region32_fini(&region);
			return;
		}
		surface->surface_node = node;
		surface->surface_node_destroy.notify =
			handle_surface_node_destroy;
		wl_signal_add(&node->events.destroy,
			&surface->surface_node_destroy);
	}
	if (!node) {
		pixman_region32_fini(&region);
		return;
	}

	/* Blur nodes live in the tree holding the surface node */
	struct wlr_scene_tree *tree = node->parent;
	int offset_x = node->x;
	int offset_y = node->y;

	if (!wl_list_empty(&surface->blur_nodes)
			&& surface->applied_tree == tree
			&& surface->applied_x == offset_x
			&& surface->applied_y == offset_y
			&& pixman_region32_equal(&surface->applied, &region)) {
		/* Nothing changed */
		pixman_region32_fini(&region);
		return;
	}

	blur_nodes_destroy(surface);

	int n_rects;
	const pixman_box32_t *rects =
		pixman_region32_rectangles(&region, &n_rects);
	for (int i = 0; i < n_rects; i++) {
		int x = offset_x + rects[i].x1;
		int y = offset_y + rects[i].y1;
		int width = rects[i].x2 - rects[i].x1;
		int height = rects[i].y2 - rects[i].y1;

		struct wlr_scene_blur *blur =
			wlr_scene_blur_create(tree, width, height);
		if (!blur) {
			wlr_log(WLR_ERROR, "failed to create blur node");
			continue;
		}
		wlr_scene_node_set_position(&blur->node, x, y);
		wlr_scene_blur_set_strength(blur, rc.blur.strength);
		/* Render below the surface content, above everything else */
		wlr_scene_node_lower_to_bottom(&blur->node);

		struct blur_node *blur_node = znew(*blur_node);
		blur_node->blur = blur;
		blur_node->destroy.notify = handle_blur_node_destroy;
		wl_signal_add(&blur->node.events.destroy, &blur_node->destroy);
		wl_list_insert(&surface->blur_nodes, &blur_node->link);
	}

	pixman_region32_copy(&surface->applied, &region);
	surface->applied_tree = tree;
	surface->applied_x = offset_x;
	surface->applied_y = offset_y;
	const pixman_box32_t *extents = pixman_region32_extents(&region);
	wlr_log(WLR_DEBUG, "ext-background-effect-v1: created %d blur "
		"node(s) for surface %p covering %dx%d",
		n_rects, (void *)surface->wlr_surface,
		extents->x2 - extents->x1, extents->y2 - extents->y1);
	pixman_region32_fini(&region);
}

static void
background_effect_surface_destroy(struct background_effect_surface *surface)
{
	if (!surface) {
		return;
	}
	blur_nodes_destroy(surface);
	if (surface->surface_node) {
		wl_list_remove(&surface->surface_node_destroy.link);
	}
	wl_list_remove(&surface->link);
	pixman_region32_fini(&surface->applied);
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

static void
surface_synced_commit(struct wlr_surface_synced *synced)
{
	struct background_effect_surface *surface =
		wl_container_of(synced, surface, synced);
	blur_nodes_update(surface);
}

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct background_effect_surface_state),
	.init_state = surface_synced_init_state,
	.finish_state = surface_synced_finish_state,
	.move_state = surface_synced_move_state,
	.commit = surface_synced_commit,
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
	wl_list_init(&surface->blur_nodes);
	pixman_region32_init(&surface->applied);
	wl_list_insert(&surfaces, &surface->link);

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

void
background_effect_reconfigure(void)
{
	if (server.scene) {
		wlr_scene_set_blur_data(server.scene, rc.blur.passes,
			rc.blur.radius, rc.blur.noise, rc.blur.brightness,
			rc.blur.contrast, rc.blur.saturation);
	}

	struct background_effect_surface *surface;
	wl_list_for_each(surface, &surfaces, link) {
		struct blur_node *blur_node;
		wl_list_for_each(blur_node, &surface->blur_nodes, link) {
			wlr_scene_blur_set_strength(blur_node->blur,
				rc.blur.strength);
		}
	}

	/* The frame on screen may still show the previous parameters */
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		wlr_output_schedule_frame(output->wlr_output);
	}

	wlr_log(WLR_DEBUG, "ext-background-effect-v1: blur parameters: "
		"passes=%d radius=%d noise=%.3f brightness=%.2f contrast=%.2f "
		"saturation=%.2f strength=%.2f", rc.blur.passes, rc.blur.radius,
		rc.blur.noise, rc.blur.brightness, rc.blur.contrast,
		rc.blur.saturation, rc.blur.strength);
}
