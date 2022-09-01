// SPDX-License-Identifier: GPL-2.0-only
/*
 * layers.c - layer-shell implementation
 *
 * Based on
 *  - https://git.sr.ht/~sircmpwm/wio
 *  - https://github.com/swaywm/sway
 * Copyright (C) 2019 Drew DeVault and Sway developers
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/log.h>
#include "layers.h"
#include "labwc.h"
#include "node.h"

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

	int nr_layers = sizeof(output->layers) / sizeof(output->layers[0]);
	for (int i = 0; i < nr_layers; i++) {
		struct lab_layer_surface *lab_layer_surface;

		/*
		 * First we go over the list of surfaces that have
		 * exclusive_zone set (e.g. statusbars) because we have to
		 * determine the usable area before processing regular layouts.
		 */
		wl_list_for_each(lab_layer_surface, &output->layers[i], link) {
			struct wlr_scene_layer_surface_v1 *scene_layer_surface =
				lab_layer_surface->scene_layer_surface;
			if (scene_layer_surface->layer_surface->current.exclusive_zone) {
				wlr_scene_layer_surface_v1_configure(
					scene_layer_surface, &full_area, &usable_area);
			}
		}

		/* Now we process regular layouts */
		wl_list_for_each(lab_layer_surface, &output->layers[i], link) {
			struct wlr_scene_layer_surface_v1 *scene_layer_surface =
				lab_layer_surface->scene_layer_surface;
			if (!scene_layer_surface->layer_surface->current.exclusive_zone) {
				wlr_scene_layer_surface_v1_configure(
					scene_layer_surface, &full_area, &usable_area);
			}
		}

		wlr_scene_node_set_position(&output->layer_tree[i]->node,
			scene_output->x, scene_output->y);
	}

	memcpy(&output->usable_area, &usable_area, sizeof(struct wlr_box));

	/* Find topmost keyboard interactive layer, if such a layer exists */
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	size_t nlayers = sizeof(layers_above_shell)
		/ sizeof(layers_above_shell[0]);
	struct lab_layer_surface *layer, *topmost = NULL;
	for (size_t i = 0; i < nlayers; ++i) {
		wl_list_for_each_reverse (layer,
				&output->layers[layers_above_shell[i]], link) {
			struct wlr_layer_surface_v1 *layer_surface =
				layer->scene_layer_surface->layer_surface;
			if (layer_surface->current.keyboard_interactive) {
				topmost = layer;
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
	if (old_usable_area.width != output->usable_area.width
			|| old_usable_area.height != output->usable_area.height) {
		desktop_arrange_all_views(server);
	}
}

static void
output_destroy_notify(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer =
		wl_container_of(listener, layer, output_destroy);
	layer->scene_layer_surface->layer_surface->output = NULL;
}

static void
surface_commit_notify(struct wl_listener *listener, void *data)
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

	if (layer_surface->current.committed
			|| layer->mapped != layer_surface->mapped) {
		layer->mapped = layer_surface->mapped;
		struct output *output =
			output_from_wlr_output(layer->server, wlr_output);
		layers_arrange(output);
	}
}

static void
unmap(struct lab_layer_surface *layer)
{
	struct seat *seat = &layer->server->seat;
	if (seat->focused_layer == layer->scene_layer_surface->layer_surface) {
		seat_set_focus_layer(seat, NULL);
	}
	if (seat->pressed.surface == layer->scene_layer_surface->layer_surface->surface) {
		seat_reset_pressed(seat);
	}
}

static void
destroy_notify(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer = wl_container_of(
		listener, layer, destroy);
	unmap(layer);

	wl_list_remove(&layer->link);
	wl_list_remove(&layer->destroy.link);
	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->unmap.link);
	wl_list_remove(&layer->surface_commit.link);
	if (layer->scene_layer_surface->layer_surface->output) {
		wl_list_remove(&layer->output_destroy.link);
		struct output *output = output_from_wlr_output(layer->server,
			layer->scene_layer_surface->layer_surface->output);
		layers_arrange(output);
	}
	free(layer);
}

static void
unmap_notify(struct wl_listener *listener, void *data)
{
	return;
	struct lab_layer_surface *lab_layer_surface =
		wl_container_of(listener, lab_layer_surface, unmap);
	unmap(lab_layer_surface);
}

static void
map_notify(struct wl_listener *listener, void *data)
{
	return;
	struct wlr_layer_surface_v1 *layer_surface = data;
	wlr_surface_send_enter(layer_surface->surface, layer_surface->output);
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
	struct lab_layer_popup *popup =
		calloc(1, sizeof(struct lab_layer_popup));
	if (!popup) {
		return NULL;
	}

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
	struct output *output = output_from_wlr_output(server, wlr_output);
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
new_popup_notify(struct wl_listener *listener, void *data)
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
new_layer_surface_notify(struct wl_listener *listener, void *data)
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

	struct lab_layer_surface *surface =
		calloc(1, sizeof(struct lab_layer_surface));
	if (!surface) {
		return;
	}

	surface->surface_commit.notify = surface_commit_notify;
	wl_signal_add(&layer_surface->surface->events.commit,
		&surface->surface_commit);

	surface->destroy.notify = destroy_notify;
	wl_signal_add(&layer_surface->events.destroy, &surface->destroy);

	surface->map.notify = map_notify;
	wl_signal_add(&layer_surface->events.map, &surface->map);

	surface->unmap.notify = unmap_notify;
	wl_signal_add(&layer_surface->events.unmap, &surface->unmap);

	surface->new_popup.notify = new_popup_notify;
	wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);

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

	surface->output_destroy.notify = output_destroy_notify;
	wl_signal_add(&layer_surface->output->events.destroy,
		&surface->output_destroy);

	if (!output) {
		wlr_log(WLR_ERROR, "no output for layer");
		return;
	}

	wl_list_insert(output->layers[layer_surface->pending.layer].prev,
		&surface->link);
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
	server->new_layer_surface.notify = new_layer_surface_notify;
	wl_signal_add(&server->layer_shell->events.new_surface,
		&server->new_layer_surface);
}
