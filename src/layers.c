// SPDX-License-Identifier: GPL-2.0-only
/*
 * layers.c - layer-shell implementation
 *
 * Based on https://github.com/swaywm/sway
 * Copyright (C) 2019 Drew DeVault and Sway developers
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "layers.h"
#include "labwc.h"
#include "node.h"

#define LAB_LAYERSHELL_VERSION 4

static void
apply_override(struct output *output, struct wlr_box *usable_area)
{
	struct usable_area_override *override;
	wl_list_for_each(override, &rc.usable_area_overrides, link) {
		if (override->output && strcasecmp(override->output, output->wlr_output->name)) {
			continue;
		}
		usable_area->x += override->margin.left;
		usable_area->y += override->margin.top;
		usable_area->width -= override->margin.left + override->margin.right;
		usable_area->height -= override->margin.top + override->margin.bottom;
	}
}

static void
arrange_one_layer(const struct wlr_box *full_area, struct wlr_box *usable_area,
		struct wlr_scene_tree *tree, bool exclusive)
{
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		struct lab_layer_surface *surface = node_layer_surface_from_node(node);
		struct wlr_scene_layer_surface_v1 *scene = surface->scene_layer_surface;
		if (!!scene->layer_surface->current.exclusive_zone != exclusive) {
			continue;
		}
		wlr_scene_layer_surface_v1_configure(scene, full_area, usable_area);
	}
}

/*
 * To ensure outputs/views are left in a consistent state, this
 * function should be called ONLY from output_update_usable_area()
 * or output_update_all_usable_areas().
 */
void
layers_arrange(struct output *output)
{
	assert(output);
	struct wlr_box full_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
		&full_area.width, &full_area.height);
	struct wlr_box usable_area = full_area;

	apply_override(output, &usable_area);

	struct server *server = output->server;
	struct wlr_scene_output *scene_output =
		wlr_scene_get_scene_output(server->scene, output->wlr_output);
	if (!scene_output) {
		wlr_log(WLR_DEBUG, "no wlr_scene_output");
		return;
	}

	for (int i = ARRAY_SIZE(output->layer_tree) - 1; i >= 0; i--) {
		struct wlr_scene_tree *layer = output->layer_tree[i];

		/*
		 * Process exclusive-zone clients before non-exclusive-zone
		 * clients, so that the latter give way to the former regardless
		 * of the order in which they were launched.
		 *
		 * Also start calculating the usable_area for exclusive-zone
		 * clients from the Overlay layer down to the Background layer
		 * to ensure that higher layers have a higher preference for
		 * placement.
		 *
		 * The 'exclusive' boolean also matches -1 which means that
		 * the layershell client wants to use the full screen rather
		 * than the usable area.
		 */
		arrange_one_layer(&full_area, &usable_area, layer, /* exclusive */ true);
	}

	for (size_t i = 0; i < ARRAY_SIZE(output->layer_tree); i++) {
		struct wlr_scene_tree *layer = output->layer_tree[i];
		arrange_one_layer(&full_area, &usable_area, layer, /* exclusive */ false);

		/* Set node position to account for output layout change */
		wlr_scene_node_set_position(&layer->node, scene_output->x,
			scene_output->y);
	}

	output->usable_area = usable_area;
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer =
		wl_container_of(listener, layer, output_destroy);
	layer->scene_layer_surface->layer_surface->output = NULL;
	wlr_layer_surface_v1_destroy(layer->scene_layer_surface->layer_surface);
}

static inline bool
has_exclusive_interactivity(struct wlr_scene_layer_surface_v1 *scene)
{
	return scene->layer_surface->current.keyboard_interactive
		== ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
}

/*
 * Try to transfer focus to other layer-shell clients with exclusive focus on
 * the output nearest to the cursor. If none exist (which is likely to generally
 * be the case) just unset layer focus and try to give it to the topmost
 * toplevel if one exists.
 */
static void
try_to_focus_next_layer_or_toplevel(struct server *server)
{
	struct seat *seat = &server->seat;
	struct output *output = output_nearest_to_cursor(server);

	enum zwlr_layer_shell_v1_layer overlay = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
	enum zwlr_layer_shell_v1_layer top = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	for (size_t i = overlay; i >= top; i--) {
		struct wlr_scene_tree *tree = output->layer_tree[i];
		struct wlr_scene_node *node;
		/*
		 * In wlr_scene.c they were added at end of list so we
		 * iterate in reverse to process last client first.
		 */
		wl_list_for_each_reverse(node, &tree->children, link) {
			struct lab_layer_surface *layer = node_layer_surface_from_node(node);
			struct wlr_scene_layer_surface_v1 *scene = layer->scene_layer_surface;
			struct wlr_layer_surface_v1 *layer_surface = scene->layer_surface;
			/*
			 * In case we have just come from the unmap handler and
			 * the commit has not yet been processed.
			 */
			if (!layer_surface->surface->mapped) {
				continue;
			}
			if (has_exclusive_interactivity(scene)) {
				wlr_log(WLR_DEBUG, "focus next exclusive layer client");
				seat_set_focus_layer(seat, layer_surface);
				return;
			}
		}
	}

	/*
	 * Unfocus the current layer-surface and focus the topmost toplevel if
	 * one exists on the current workspace.
	 */
	if (seat->focused_layer) {
		seat_set_focus_layer(seat, NULL);
	}
}

static bool
focused_layer_has_exclusive_interactivity(struct seat *seat)
{
	if (!seat->focused_layer) {
		return false;
	}
	return seat->focused_layer->current.keyboard_interactive ==
		ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
}

/*
 * Precedence is defined as being in the same or higher (overlay is highest)
 * than the layer with current keyboard focus.
 */
static bool
has_precedence(struct seat *seat, enum zwlr_layer_shell_v1_layer layer)
{
	if (!seat->focused_layer) {
		return true;
	}
	if (!focused_layer_has_exclusive_interactivity(seat)) {
		return true;
	}
	if (layer >= seat->focused_layer->current.layer) {
		return true;
	}
	return false;
}

void
layer_try_set_focus(struct seat *seat, struct wlr_layer_surface_v1 *layer_surface)
{
	switch (layer_surface->current.keyboard_interactive) {
	case ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE:
		wlr_log(WLR_DEBUG, "interactive-exclusive '%p'", layer_surface);
		if (has_precedence(seat, layer_surface->current.layer)) {
			seat_set_focus_layer(seat, layer_surface);
		}
		break;
	case ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND:
		wlr_log(WLR_DEBUG, "interactive-on-demand '%p'", layer_surface);
		if (!focused_layer_has_exclusive_interactivity(seat)) {
			seat_set_focus_layer(seat, layer_surface);
		}
		break;
	case ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE:
		wlr_log(WLR_DEBUG, "interactive-none '%p'", layer_surface);
		if (seat->focused_layer == layer_surface) {
			try_to_focus_next_layer_or_toplevel(seat->server);
		}
		break;
	}
}

static bool
is_on_demand(struct wlr_layer_surface_v1 *layer_surface)
{
	return layer_surface->current.keyboard_interactive ==
		ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;
}

static void
handle_surface_commit(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer =
		wl_container_of(listener, layer, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface =
		layer->scene_layer_surface->layer_surface;
	struct wlr_output *wlr_output =
		layer->scene_layer_surface->layer_surface->output;

	if (!wlr_output) {
		return;
	}

	uint32_t committed = layer_surface->current.committed;
	struct output *output = (struct output *)wlr_output->data;

	/* Process layer change */
	if (committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
		wlr_scene_node_reparent(&layer->scene_layer_surface->tree->node,
			output->layer_tree[layer_surface->current.layer]);
	}
	/* Process keyboard-interactivity change */
	if (committed & WLR_LAYER_SURFACE_V1_STATE_KEYBOARD_INTERACTIVITY) {
		/*
		 * On-demand interactivity should only be honoured through
		 * normal focus semantics (for example by surface receiving
		 * cursor-button-press).
		 */
		if (is_on_demand(layer_surface)) {
			struct seat *seat = &layer->server->seat;
			if (seat->focused_layer == layer_surface) {
				/*
				 * Must be change from EXCLUSIVE to ON_DEMAND,
				 * so we should give us focus.
				 */
				struct server *server = layer->server;
				try_to_focus_next_layer_or_toplevel(server);
			}
			goto out;
		}
		/* Handle EXCLUSIVE and NONE requests */
		struct seat *seat = &layer->server->seat;
		layer_try_set_focus(seat, layer_surface);
	}
out:

	if (committed || layer->mapped != layer_surface->surface->mapped) {
		layer->mapped = layer_surface->surface->mapped;
		output_update_usable_area(output);
		/*
		 * Update cursor focus here to ensure we
		 * enter a new/moved/resized layer surface.
		 */
		cursor_update_focus(layer->server);
	}
}

static void
handle_node_destroy(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer =
		wl_container_of(listener, layer, node_destroy);
	/*
	 * TODO: Determine if this layer is being used by an exclusive client.
	 * If it is, try and find another layer owned by this client to pass
	 * focus to.
	 */

	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->unmap.link);
	wl_list_remove(&layer->surface_commit.link);
	wl_list_remove(&layer->new_popup.link);
	wl_list_remove(&layer->output_destroy.link);
	wl_list_remove(&layer->node_destroy.link);
	free(layer);
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer = wl_container_of(listener, layer, unmap);
	struct wlr_layer_surface_v1 *layer_surface =
		layer->scene_layer_surface->layer_surface;
	if (layer_surface->output) {
		output_update_usable_area(layer_surface->output->data);
	}
	struct seat *seat = &layer->server->seat;
	if (seat->focused_layer == layer_surface) {
		try_to_focus_next_layer_or_toplevel(layer->server);
	}
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer = wl_container_of(listener, layer, map);
	struct wlr_output *wlr_output =
		layer->scene_layer_surface->layer_surface->output;
	if (wlr_output) {
		output_update_usable_area(wlr_output->data);
	}

	/*
	 * Since moving to the wlroots scene-graph API, there is no need to
	 * call wlr_surface_send_enter() from here since that will be done
	 * automatically based on the position of the surface and outputs in
	 * the scene. See wlr_scene_surface_create() documentation.
	 */

	struct seat *seat = &layer->server->seat;
	layer_try_set_focus(seat, layer->scene_layer_surface->layer_surface);
}

static void
popup_handle_destroy(struct wl_listener *listener, void *data)
{
	struct lab_layer_popup *popup =
		wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->reposition.link);

	/* Usually already removed unless there was no commit at all */
	if (popup->commit.notify) {
		wl_list_remove(&popup->commit.link);
	}

	cursor_update_focus(popup->server);

	free(popup);
}

static void
popup_handle_commit(struct wl_listener *listener, void *data)
{
	struct lab_layer_popup *popup =
		wl_container_of(listener, popup, commit);

	if (popup->wlr_popup->base->initial_commit) {
		wlr_xdg_popup_unconstrain_from_box(popup->wlr_popup,
			&popup->output_toplevel_sx_box);

		/* Prevent getting called over and over again */
		wl_list_remove(&popup->commit.link);
		popup->commit.notify = NULL;
	}
}

static void
popup_handle_reposition(struct wl_listener *listener, void *data)
{
	struct lab_layer_popup *popup =
		wl_container_of(listener, popup, reposition);
	wlr_xdg_popup_unconstrain_from_box(popup->wlr_popup,
		&popup->output_toplevel_sx_box);
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data);

static struct lab_layer_popup *
create_popup(struct server *server, struct wlr_xdg_popup *wlr_popup,
		struct wlr_scene_tree *parent)
{
	struct lab_layer_popup *popup = znew(*popup);
	popup->server = server;
	popup->wlr_popup = wlr_popup;
	popup->scene_tree =
		wlr_scene_xdg_surface_create(parent, wlr_popup->base);
	if (!popup->scene_tree) {
		free(popup);
		return NULL;
	}

	/* In support of IME popup */
	wlr_popup->base->surface->data = popup->scene_tree;

	node_descriptor_create(&popup->scene_tree->node,
		LAB_NODE_DESC_LAYER_POPUP, popup);

	popup->destroy.notify = popup_handle_destroy;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);

	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

	popup->commit.notify = popup_handle_commit;
	wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

	popup->reposition.notify = popup_handle_reposition;
	wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);

	return popup;
}

/* This popup's parent is a layer popup */
static void
popup_handle_new_popup(struct wl_listener *listener, void *data)
{
	struct lab_layer_popup *lab_layer_popup =
		wl_container_of(listener, lab_layer_popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	struct lab_layer_popup *new_popup = create_popup(
		lab_layer_popup->server, wlr_popup,
		lab_layer_popup->scene_tree);

	if (!new_popup) {
		wl_resource_post_no_memory(wlr_popup->resource);
		wlr_xdg_popup_destroy(wlr_popup);
		return;
	}

	new_popup->output_toplevel_sx_box =
		lab_layer_popup->output_toplevel_sx_box;
}

/*
 * We move popups from the bottom to the top layer so that they are
 * rendered above views.
 */
static void
move_popup_to_top_layer(struct lab_layer_surface *toplevel,
		struct lab_layer_popup *popup)
{
	struct server *server = toplevel->server;
	struct wlr_output *wlr_output =
		toplevel->scene_layer_surface->layer_surface->output;
	struct output *output = (struct output *)wlr_output->data;
	struct wlr_box box = { 0 };
	wlr_output_layout_get_box(server->output_layout, wlr_output, &box);
	int lx = toplevel->scene_layer_surface->tree->node.x + box.x;
	int ly = toplevel->scene_layer_surface->tree->node.y + box.y;

	struct wlr_scene_node *node = &popup->scene_tree->node;
	wlr_scene_node_reparent(node, output->layer_popup_tree);
	/* FIXME: verify the whole tree should be repositioned */
	wlr_scene_node_set_position(&output->layer_popup_tree->node, lx, ly);
}

/* This popup's parent is a shell-layer surface */
static void
handle_new_popup(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *toplevel =
		wl_container_of(listener, toplevel, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;

	struct server *server = toplevel->server;
	struct wlr_scene_layer_surface_v1 *surface = toplevel->scene_layer_surface;
	struct output *output = surface->layer_surface->output->data;

	int lx, ly;
	wlr_scene_node_coords(&surface->tree->node, &lx, &ly);

	struct wlr_box output_box = { 0 };
	wlr_output_layout_get_box(server->output_layout,
		output->wlr_output, &output_box);

	/*
	 * Output geometry expressed in the coordinate system of the toplevel
	 * parent of popup. We store this struct the lab_layer_popup struct
	 * to make it easier to unconstrain children when we move popups from
	 * the bottom to the top layer.
	 */
	struct wlr_box output_toplevel_sx_box = {
		.x = output_box.x - lx,
		.y = output_box.y - ly,
		.width = output_box.width,
		.height = output_box.height,
	};
	struct lab_layer_popup *popup =
		create_popup(server, wlr_popup, surface->tree);
	if (!popup) {
		wl_resource_post_no_memory(wlr_popup->resource);
		wlr_xdg_popup_destroy(wlr_popup);
		return;
	}

	popup->output_toplevel_sx_box = output_toplevel_sx_box;

	if (surface->layer_surface->current.layer
			<= ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM) {
		move_popup_to_top_layer(toplevel, popup);
	}
}

static void
handle_new_layer_surface(struct wl_listener *listener, void *data)
{
	struct server *server = wl_container_of(
		listener, server, new_layer_surface);
	struct wlr_layer_surface_v1 *layer_surface = data;

	if (!layer_surface->output) {
		struct wlr_output *output = wlr_output_layout_output_at(
			server->output_layout, server->seat.cursor->x,
			server->seat.cursor->y);
		if (!output) {
			wlr_log(WLR_INFO,
				"No output available to assign layer surface");
			wlr_layer_surface_v1_destroy(layer_surface);
			return;
		}
		layer_surface->output = output;
	}

	struct lab_layer_surface *surface = znew(*surface);

	struct output *output = layer_surface->output->data;

	struct wlr_scene_tree *selected_layer =
		output->layer_tree[layer_surface->current.layer];

	surface->scene_layer_surface = wlr_scene_layer_surface_v1_create(
		selected_layer, layer_surface);
	if (!surface->scene_layer_surface) {
		wlr_layer_surface_v1_destroy(layer_surface);
		wlr_log(WLR_ERROR, "could not create layer surface");
		return;
	}

	/* In support of IME popup */
	layer_surface->surface->data = surface->scene_layer_surface->tree;

	node_descriptor_create(&surface->scene_layer_surface->tree->node,
		LAB_NODE_DESC_LAYER_SURFACE, surface);

	surface->server = server;
	surface->scene_layer_surface->layer_surface = layer_surface;

	surface->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit,
		&surface->surface_commit);

	surface->map.notify = handle_map;
	wl_signal_add(&layer_surface->surface->events.map, &surface->map);

	surface->unmap.notify = handle_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &surface->unmap);

	surface->new_popup.notify = handle_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);

	surface->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&layer_surface->output->events.destroy,
		&surface->output_destroy);

	surface->node_destroy.notify = handle_node_destroy;
	wl_signal_add(&surface->scene_layer_surface->tree->node.events.destroy,
		&surface->node_destroy);
}

void
layers_init(struct server *server)
{
	server->layer_shell = wlr_layer_shell_v1_create(server->wl_display,
		LAB_LAYERSHELL_VERSION);
	server->new_layer_surface.notify = handle_new_layer_surface;
	wl_signal_add(&server->layer_shell->events.new_surface,
		&server->new_layer_surface);
}
