// SPDX-License-Identifier: GPL-2.0-only
/*
 * layers.c - layer-shell implementation
 *
 * Based on:
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

void
arrange_layers(struct output *output)
{
	struct server *server = output->server;
	struct wlr_scene_output *scene_output =
		wlr_scene_get_scene_output(server->scene, output->wlr_output);

	struct wlr_box full_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
			&full_area.width, &full_area.height);
	struct wlr_box usable_area = full_area;

	for (int i = 0; i < 4; i++) {
		struct lab_layer_surface *lab_layer_surface;
		wl_list_for_each(lab_layer_surface, &output->layers[i], link) {
			struct wlr_scene_layer_surface_v1 *scene_layer_surface =
				lab_layer_surface->scene_layer_surface;
			wlr_scene_layer_surface_v1_configure(
				scene_layer_surface, &full_area, &usable_area);
		}
	}

	memcpy(&output->usable_area, &usable_area, sizeof(struct wlr_box));

	/* Find topmost keyboard interactive layer, if such a layer exists */
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	size_t nlayers = sizeof(layers_above_shell) / sizeof(layers_above_shell[0]);
	struct lab_layer_surface *layer, *topmost = NULL;
	for (size_t i = 0; i < nlayers; ++i) {
		wl_list_for_each_reverse (layer,
				&output->layers[layers_above_shell[i]], link) {
			if (layer->scene_layer_surface->layer_surface->current.keyboard_interactive) {
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
	/* FIXME: should we call a desktop_arrange_all_views() here? */
}

static void
output_destroy_notify(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer =
		wl_container_of(listener, layer, output_destroy);
	wl_list_remove(&layer->output_destroy.link);
	wlr_layer_surface_v1_destroy(layer->scene_layer_surface->layer_surface);
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
		arrange_layers(output);
	}
}

static void
unmap(struct lab_layer_surface *layer)
{
	struct seat *seat = &layer->server->seat;
	if (seat->focused_layer == layer->scene_layer_surface->layer_surface) {
		seat_set_focus_layer(seat, NULL);
	}
	damage_all_outputs(layer->server);
}

static void
destroy_notify(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *layer = wl_container_of(
		listener, layer, destroy);
	if (layer->scene_layer_surface->layer_surface->mapped) {
		unmap(layer);
	}

	/* TODO: sort this out properly */
	wl_list_remove(&layer->link);
	wl_list_remove(&layer->destroy.link);
	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->surface_commit.link);
	if (layer->scene_layer_surface->layer_surface->output) {
		wl_list_remove(&layer->output_destroy.link);
		struct output *output = output_from_wlr_output(
			layer->server, layer->scene_layer_surface->layer_surface->output);
		arrange_layers(output);
	}
	free(layer);
}

static void
unmap_notify(struct wl_listener *listener, void *data)
{
	struct lab_layer_surface *l = wl_container_of(listener, l, unmap);
	unmap(l);
}

static void
map_notify(struct wl_listener *listener, void *data)
{
	struct wlr_layer_surface_v1 *l = data;
	wlr_surface_send_enter(l->surface, l->output);
}

//static struct
//lab_layer_surface *popup_get_layer(struct lab_layer_popup *popup)
//{
//	while (popup->parent_type == LAYER_PARENT_POPUP) {
//		popup = popup->parent_popup;
//	}
//	return popup->parent_layer;
//}
//
//static void
//popup_damage(struct lab_layer_popup *layer_popup, bool whole)
//{
//	struct lab_layer_surface *layer;
//	while (true) {
//		if (layer_popup->parent_type == LAYER_PARENT_POPUP) {
//			layer_popup = layer_popup->parent_popup;
//		} else {
//			layer = layer_popup->parent_layer;
//			break;
//		}
//	}
//	damage_all_outputs(layer->server);
//}
//
//static void
//popup_handle_map(struct wl_listener *listener, void *data)
//{
//	struct lab_layer_popup *popup = wl_container_of(listener, popup, map);
//	struct lab_layer_surface *layer = popup_get_layer(popup);
//	struct wlr_output *wlr_output = layer->scene_layer_surface->layer_surface->output;
//	wlr_surface_send_enter(popup->wlr_popup->base->surface, wlr_output);
//	popup_damage(popup, true);
//}
//
//static void
//popup_handle_unmap(struct wl_listener *listener, void *data)
//{
//	struct lab_layer_popup *popup = wl_container_of(listener, popup, unmap);
//	popup_damage(popup, true);
//}
//
//static void
//popup_handle_commit(struct wl_listener *listener, void *data)
//{
//	struct lab_layer_popup *popup = wl_container_of(listener, popup, commit);
//	popup_damage(popup, false);
//}
//
//static void
//popup_handle_destroy(struct wl_listener *listener, void *data)
//{
//	struct lab_layer_popup *popup =
//		wl_container_of(listener, popup, destroy);
//	wl_list_remove(&popup->destroy.link);
//	free(popup);
//}
//
//static void
//popup_unconstrain(struct lab_layer_popup *popup)
//{
//	struct lab_layer_surface *layer = popup_get_layer(popup);
//	struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;
//	struct output *output = layer->scene_layer_surface->layer_surface->output->data;
//
//	struct wlr_box output_box = { 0 };
//	wlr_output_effective_resolution(output->wlr_output, &output_box.width,
//		&output_box.height);
//
//	struct wlr_box output_toplevel_sx_box = {
//		.x = -layer->geo.x,
//		.y = -layer->geo.y,
//		.width = output_box.width,
//		.height = output_box.height,
//	};
//
//	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
//}
//
//static void popup_handle_new_popup(struct wl_listener *listener, void *data);
//
//static struct lab_layer_popup *
//create_popup(struct wlr_xdg_popup *wlr_popup,
//		enum layer_parent parent_type, void *parent)
//{
//	struct lab_layer_popup *popup =
//		calloc(1, sizeof(struct lab_layer_popup));
//	if (!popup) {
//		return NULL;
//	}
//
//	struct lab_layer_surface *layer = parent_type == LAYER_PARENT_LAYER
//			? (struct lab_layer_surface *)parent
//			: (struct lab_layer_popup *)parent;
//	struct server *server = layer->server;
//
//	popup->wlr_popup = wlr_popup;
//	popup->parent_type = parent_type;
//	popup->parent_layer = parent;
//
//	popup->destroy.notify = popup_handle_destroy;
//	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
//	popup->new_popup.notify = popup_handle_new_popup;
//	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);
//
//	if (!wlr_surface_is_layer_surface(wlr_popup->base->surface)) {
//		wlr_log(WLR_ERROR, "xdg_surface is not layer surface");
//		return;
//	}
//
//	struct wlr_output *wlr_output =
//		layer->scene_layer_surface->layer_surface->data;
//	struct output *output = output_from_wlr_output(server, wlr_output);
//
//	struct wlr_scene_tree *selected_layer =
//		output->layer_tree[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY];
//	struct wlr_scene_node *node =
//		wlr_scene_layer_surface_v1_create(&server->view_tree->node,
//			wlr_popup->base->surface->data);
//	wlr_popup->base->surface->data =
//		wlr_scene_xdg_surface_create(&selected_layer->node, wlr_popup->base);
//
//	popup_unconstrain(popup);
//
//	return popup;
//}
//
//static void
//popup_handle_new_popup(struct wl_listener *listener, void *data)
//{
//	struct lab_layer_popup *lab_layer_popup =
//		wl_container_of(listener, lab_layer_popup, new_popup);
//	struct wlr_xdg_popup *wlr_popup = data;
//	create_popup(wlr_popup, LAYER_PARENT_POPUP, lab_layer_popup);
//}
//
//static void
//new_popup_notify(struct wl_listener *listener, void *data)
//{
//	struct lab_layer_surface *lab_layer_surface =
//		wl_container_of(listener, lab_layer_surface, new_popup);
//	struct wlr_xdg_popup *wlr_popup = data;
//	create_popup(wlr_popup, LAYER_PARENT_LAYER, lab_layer_surface);
//}

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

	/* TODO: support popups */
//	surface->new_popup.notify = new_popup_notify;
//	wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);
//
//	surface->new_subsurface.notify = new_subsurface_notify;
//	wl_signal_add(&layer_surface->surface->events.new_subsurface,
//		&surface->new_subsurface);

	struct output *output = layer_surface->output->data;

	struct wlr_scene_tree *selected_layer =
		output->layer_tree[layer_surface->current.layer];

	surface->scene_layer_surface = wlr_scene_layer_surface_v1_create(
		&selected_layer->node, layer_surface);

	surface->server = server;
	surface->scene_layer_surface->layer_surface = layer_surface;

	/* wlr_surface->data needed to find parent in xdg_surface_new() */
	layer_surface->surface->data = surface->scene_layer_surface->node;

	surface->output_destroy.notify = output_destroy_notify;
	wl_signal_add(&layer_surface->output->events.destroy,
		&surface->output_destroy);

	if (!output) {
		wlr_log(WLR_ERROR, "no output for layer");
		return;
	}

	wl_list_insert(&output->layers[layer_surface->pending.layer],
		&surface->link);
	/*
	 * Temporarily set the layer's current state to pending so that
	 * it can easily be arranged.
	 */
	struct wlr_layer_surface_v1_state old_state = layer_surface->current;
	layer_surface->current = layer_surface->pending;
	arrange_layers(output);
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
