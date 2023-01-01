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
#include <wayland-server.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "layers.h"
#include "labwc.h"
#include "node.h"

static void
arrange_one_layer(struct output *output, const struct wlr_box *full_area,
		struct wlr_box *usable_area, struct wlr_scene_tree *tree,
		bool exclusive)
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

void
layers_arrange(struct output *output)
{
	struct wlr_box full_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
		&full_area.width, &full_area.height);
	struct wlr_box usable_area = full_area;
	struct wlr_box old_usable_area = output->usable_area;

	struct server *server = output->server;
	struct wlr_scene_output *scene_output =
		wlr_scene_get_scene_output(server->scene, output->wlr_output);
	if (!scene_output) {
		wlr_log(WLR_DEBUG, "no wlr_scene_output");
		return;
	}

	int nr_layers = sizeof(output->layer_tree) / sizeof(output->layer_tree[0]);
	for (int i = 0; i < nr_layers; i++) {
		struct wlr_scene_tree *layer = output->layer_tree[i];

		/*
		 * Process exclusive-zone clients before non-exclusive-zone
		 * clients, so that the latter give way to the former regardless
		 * of the order in which they were launched.
		 */
		arrange_one_layer(output, &full_area, &usable_area, layer, true);
		arrange_one_layer(output, &full_area, &usable_area, layer, false);

		/* Set node position to account for output layout change */
		wlr_scene_node_set_position(&layer->node, scene_output->x,
			scene_output->y);
	}

	output->usable_area = usable_area;

	/* Find topmost keyboard interactive layer, if such a layer exists */
	uint32_t layers_above_views[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	size_t nlayers = sizeof(layers_above_views) / sizeof(layers_above_views[0]);
	struct lab_layer_surface *topmost = NULL;
	struct wlr_scene_node *node;
	for (size_t i = 0; i < nlayers; ++i) {
		struct wlr_scene_tree *tree = output->layer_tree[layers_above_views[i]];
		/* Iterate in reverse to give most recent node preference */
		wl_list_for_each_reverse(node, &tree->children, link) {
			struct lab_layer_surface *surface = node_layer_surface_from_node(node);
			struct wlr_scene_layer_surface_v1 *scene = surface->scene_layer_surface;
			if (scene->layer_surface->current.keyboard_interactive) {
				topmost = surface;
				break;
			}
		}
		if (topmost) {
			break;
		}
	}
	struct seat *seat = &output->server->seat;
	if (topmost) {
		seat_set_focus_layer(seat,
			topmost->scene_layer_surface->layer_surface);
	} else if (seat->focused_layer &&
			!seat->focused_layer->current.keyboard_interactive) {
		seat_set_focus_layer(seat, NULL);
	}

	/* Finally re-arrange all views based on usable_area */
	if (!wlr_box_equal(&old_usable_area, &usable_area)) {
		desktop_arrange_all_views(server);
	}
	cursor_update_focus(output->server);
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer =
		wl_container_of(listener, layer, output_destroy);
	layer->scene_layer_surface->layer_surface->output = NULL;
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

	if (committed || layer->mapped != layer_surface->mapped) {
		layer->mapped = layer_surface->mapped;
		layers_arrange(output);
	}
}

static void
handle_node_destroy(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer =
		wl_container_of(listener, layer, node_destroy);

	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->unmap.link);
	wl_list_remove(&layer->surface_commit.link);
	wl_list_remove(&layer->output_destroy.link);
	wl_list_remove(&layer->node_destroy.link);
	free(layer);
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer = wl_container_of(listener, layer, unmap);
	layers_arrange(layer->scene_layer_surface->layer_surface->output->data);
	struct seat *seat = &layer->server->seat;
	if (seat->focused_layer == layer->scene_layer_surface->layer_surface) {
		seat_set_focus_layer(seat, NULL);
	}
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer = wl_container_of(listener, layer, map);
	layers_arrange(layer->scene_layer_surface->layer_surface->output->data);
	/*
	 * Since moving to the wlroots scene-graph API, there is no need to
	 * call wlr_surface_send_enter() from here since that will be done
	 * automatically based on the position of the surface and outputs in
	 * the scene. See wlr_scene_surface_create() documentation.
	 */
}

static void
popup_handle_destroy(struct wl_listener *listener, void *data)
{
	struct lab_layer_popup *popup =
		wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->new_popup.link);
	free(popup);
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data);

static struct lab_layer_popup *
create_popup(struct wlr_xdg_popup *wlr_popup, struct wlr_scene_tree *parent,
		struct wlr_box *output_toplevel_sx_box)
{
	struct lab_layer_popup *popup = znew(*popup);
	popup->wlr_popup = wlr_popup;
	popup->scene_tree =
		wlr_scene_xdg_surface_create(parent, wlr_popup->base);
	if (!popup->scene_tree) {
		free(popup);
		return NULL;
	}
	node_descriptor_create(&popup->scene_tree->node,
		LAB_NODE_DESC_LAYER_POPUP, popup);

	popup->destroy.notify = popup_handle_destroy;
	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, output_toplevel_sx_box);
	return popup;
}

/* This popup's parent is a layer popup */
static void
popup_handle_new_popup(struct wl_listener *listener, void *data)
{
	struct lab_layer_popup *lab_layer_popup =
		wl_container_of(listener, lab_layer_popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	struct lab_layer_popup *new_popup = create_popup(wlr_popup,
		lab_layer_popup->scene_tree,
		&lab_layer_popup->output_toplevel_sx_box);
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

	int lx, ly;
	struct server *server = toplevel->server;
	struct wlr_scene_layer_surface_v1 *surface = toplevel->scene_layer_surface;
	wlr_scene_node_coords(&surface->tree->node, &lx, &ly);

	if (!surface->layer_surface->output) {
		/* Work-around for moving layer shell surfaces on output destruction */
		struct wlr_output *wlr_output;
		wlr_output = wlr_output_layout_output_at(server->output_layout, lx, ly);
		surface->layer_surface->output = wlr_output;
	}
	struct output *output = surface->layer_surface->output->data;

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
	struct lab_layer_popup *popup = create_popup(wlr_popup,
		surface->tree, &output_toplevel_sx_box);
	popup->output_toplevel_sx_box = output_toplevel_sx_box;

	if (surface->layer_surface->current.layer
			== ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM) {
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

	node_descriptor_create(&surface->scene_layer_surface->tree->node,
		LAB_NODE_DESC_LAYER_SURFACE, surface);

	surface->server = server;
	surface->scene_layer_surface->layer_surface = layer_surface;

	surface->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit,
		&surface->surface_commit);

	surface->map.notify = handle_map;
	wl_signal_add(&layer_surface->events.map, &surface->map);

	surface->unmap.notify = handle_unmap;
	wl_signal_add(&layer_surface->events.unmap, &surface->unmap);

	surface->new_popup.notify = handle_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);

	surface->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&layer_surface->output->events.destroy,
		&surface->output_destroy);

	surface->node_destroy.notify = handle_node_destroy;
	wl_signal_add(&surface->scene_layer_surface->tree->node.events.destroy,
		&surface->node_destroy);

	if (!output) {
		wlr_log(WLR_ERROR, "no output for layer");
		return;
	}

	/*
	 * Temporarily set the layer's current state to pending so that
	 * it can easily be arranged.
	 */
	struct wlr_layer_surface_v1_state old_state = layer_surface->current;
	layer_surface->current = layer_surface->pending;
	layers_arrange(output);
	layer_surface->current = old_state;
}

void
layers_init(struct server *server)
{
	server->layer_shell = wlr_layer_shell_v1_create(server->wl_display);
	server->new_layer_surface.notify = handle_new_layer_surface;
	wl_signal_add(&server->layer_shell->events.new_surface,
		&server->new_layer_surface);
}
