// SPDX-License-Identifier: GPL-2.0-only
/*
 * output.c: labwc output and rendering
 *
 * Copyright (C) 2019-2021 Johan Malm
 * Copyright (C) 2020 The Sway authors
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/region.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "labwc.h"
#include "layers.h"
#include "node.h"
#include "view.h"

static void
output_frame_notify(struct wl_listener *listener, void *data)
{
	struct output *output = wl_container_of(listener, output, frame);
	if (!output->wlr_output->enabled) {
		return;
	}

	wlr_scene_output_commit(output->scene_output);

	struct timespec now = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(output->scene_output, &now);
}

static void
output_destroy_notify(struct wl_listener *listener, void *data)
{
	struct output *output = wl_container_of(listener, output, destroy);
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->destroy.link);

	int nr_layers = sizeof(output->layer_tree) / sizeof(output->layer_tree[0]);
	for (int i = 0; i < nr_layers; i++) {
		wlr_scene_node_destroy(&output->layer_tree[i]->node);
	}
	wlr_scene_node_destroy(&output->layer_popup_tree->node);
	wlr_scene_node_destroy(&output->osd_tree->node);

	struct view *view;
	struct server *server = output->server;
	wl_list_for_each(view, &server->views, link) {
		if (view->output == output) {
			view_on_output_destroy(view);
		}
	}
	free(output);
}

static void do_output_layout_change(struct server *server);

static void
new_output_notify(struct wl_listener *listener, void *data)
{
	/*
	 * This event is rasied by the backend when a new output (aka display
	 * or monitor) becomes available.
	 */
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/*
	 * We offer any display as available for lease, some apps like
	 * gamescope, want to take ownership of a display when they can
	 * to use planes and present directly.
	 * This is also useful for debugging the DRM parts of
	 * another compositor.
	 */
	if (server->drm_lease_manager) {
		wlr_drm_lease_v1_manager_offer_output(
			server->drm_lease_manager, wlr_output);
	}

	/*
	 * Don't configure any non-desktop displays, such as VR headsets;
	 */
	if (wlr_output->non_desktop) {
		wlr_log(WLR_DEBUG, "Not configuring non-desktop output");
		return;
	}

	/*
	 * Configures the output created by the backend to use our allocator
	 * and our renderer. Must be done once, before commiting the output
	 */
	if (!wlr_output_init_render(wlr_output, server->allocator,
			server->renderer)) {
		wlr_log(WLR_ERROR, "unable to init output renderer");
		return;
	}

	wlr_log(WLR_DEBUG, "enable output");
	wlr_output_enable(wlr_output, true);

	/* The mode is a tuple of (width, height, refresh rate). */
	wlr_log(WLR_DEBUG, "set preferred mode");
	struct wlr_output_mode *preferred_mode =
		wlr_output_preferred_mode(wlr_output);
	wlr_output_set_mode(wlr_output, preferred_mode);

	/*
	 * Sometimes the preferred mode is not available due to hardware
	 * constraints (e.g. GPU or cable bandwidth limitations). In these
	 * cases it's better to fallback to lower modes than to end up with
	 * a black screen. See sway@4cdc4ac6
	 */
	if (!wlr_output_test(wlr_output)) {
		wlr_log(WLR_DEBUG,
			"preferred mode rejected, falling back to another mode");
		struct wlr_output_mode *mode;
		wl_list_for_each(mode, &wlr_output->modes, link) {
			if (mode == preferred_mode) {
				continue;
			}
			wlr_output_set_mode(wlr_output, mode);
			if (wlr_output_test(wlr_output)) {
				break;
			}
		}
	}

	wlr_output_commit(wlr_output);

	struct output *output = znew(*output);
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;
	wlr_output_effective_resolution(wlr_output,
		&output->usable_area.width, &output->usable_area.height);
	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/*
	 * Create layer-trees (background, bottom, top and overlay) and
	 * a layer-popup-tree.
	 */
	int nr_layers = sizeof(output->layer_tree) / sizeof(output->layer_tree[0]);
	for (int i = 0; i < nr_layers; i++) {
		output->layer_tree[i] =
			wlr_scene_tree_create(&server->scene->tree);
		node_descriptor_create(&output->layer_tree[i]->node,
			LAB_NODE_DESC_TREE, NULL);
	}
	output->layer_popup_tree = wlr_scene_tree_create(&server->scene->tree);
	node_descriptor_create(&output->layer_popup_tree->node,
		LAB_NODE_DESC_TREE, NULL);
	output->osd_tree = wlr_scene_tree_create(&server->scene->tree);
	node_descriptor_create(&output->osd_tree->node,
		LAB_NODE_DESC_TREE, NULL);

	/*
	 * Set the z-positions to achieve the following order (from top to
	 * bottom):
	 *	- layer-shell popups
	 *	- overlay layer
	 *	- top layer
	 *	- views
	 *	- bottom layer
	 *	- background layer
	 */
	wlr_scene_node_lower_to_bottom(&output->layer_tree[1]->node);
	wlr_scene_node_lower_to_bottom(&output->layer_tree[0]->node);
	wlr_scene_node_raise_to_top(&output->layer_tree[2]->node);
	wlr_scene_node_raise_to_top(&output->layer_tree[3]->node);
	wlr_scene_node_raise_to_top(&output->layer_popup_tree->node);

	if (rc.adaptive_sync) {
		wlr_log(WLR_INFO, "enable adaptive sync on %s", wlr_output->name);
		struct wlr_output_state pending = { 0 };
		wlr_output_state_set_adaptive_sync_enabled(&pending, true);
		if (!wlr_output_test_state(wlr_output, &pending)) {
			wlr_log(WLR_ERROR, "adaptive sync failed, ignoring");
			wlr_output_state_set_adaptive_sync_enabled(&pending, false);
		}
		if (!wlr_output_commit_state(wlr_output, &pending)) {
			wlr_log(WLR_ERROR, "failed to commit output %s",
				wlr_output->name);
		}
	}

	/*
	 * Wait until wlr_output_layout_add_auto() returns before
	 * calling do_output_layout_change(); this ensures that the
	 * wlr_output_cursor is created for the new output.
	 */
	server->pending_output_layout_change++;

	wlr_output_layout_add_auto(server->output_layout, wlr_output);
	output->scene_output = wlr_scene_get_scene_output(server->scene, wlr_output);
	assert(output->scene_output);

	server->pending_output_layout_change--;
	do_output_layout_change(server);
}

void
output_init(struct server *server)
{
	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/*
	 * Create an output layout, which is a wlroots utility for working with
	 * an arrangement of screens in a physical layout.
	 */
	server->output_layout = wlr_output_layout_create();
	if (!server->output_layout) {
		wlr_log(WLR_ERROR, "unable to create output layout");
		exit(EXIT_FAILURE);
	}
	wlr_scene_attach_output_layout(server->scene, server->output_layout);

	/* Enable screen recording with wf-recorder */
	wlr_xdg_output_manager_v1_create(server->wl_display,
		server->output_layout);

	wl_list_init(&server->outputs);

	output_manager_init(server);
}

static void
output_update_for_layout_change(struct server *server)
{
	output_update_all_usable_areas(server, /*enforce_view_arrange*/ true);

	/*
	 * "Move" each wlr_output_cursor (in per-output coordinates) to
	 * align with the seat cursor. Set a default cursor image so
	 * that the cursor isn't invisible on new outputs.
	 *
	 * TODO: remember the most recent cursor image (see cursor.c)
	 * and set that rather than XCURSOR_DEFAULT
	 */
	wlr_cursor_move(server->seat.cursor, NULL, 0, 0);
	wlr_xcursor_manager_set_cursor_image(server->seat.xcursor_manager,
		XCURSOR_DEFAULT, server->seat.cursor);
}

static void
output_config_apply(struct server *server,
		struct wlr_output_configuration_v1 *config)
{
	server->pending_output_layout_change++;

	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each(head, &config->heads, link) {
		struct wlr_output *o = head->state.output;
		struct output *output = output_from_wlr_output(server, o);
		bool output_enabled = head->state.enabled && !output->leased;
		bool need_to_add = output_enabled && !o->enabled;
		bool need_to_remove = !output_enabled && o->enabled;

		wlr_output_enable(o, output_enabled);
		if (output_enabled) {
			/* Output specifc actions only */
			if (head->state.mode) {
				wlr_output_set_mode(o, head->state.mode);
			} else {
				int32_t width = head->state.custom_mode.width;
				int32_t height = head->state.custom_mode.height;
				int32_t refresh = head->state.custom_mode.refresh;
				wlr_output_set_custom_mode(o, width,
					height, refresh);
			}
			wlr_output_set_scale(o, head->state.scale);
			wlr_output_set_transform(o, head->state.transform);
		}
		if (!wlr_output_commit(o)) {
			wlr_log(WLR_ERROR, "Output config commit failed");
			continue;
		}

		/* Only do Layout specific actions if the commit went trough */
		if (need_to_add) {
			wlr_output_layout_add_auto(server->output_layout, o);
			output->scene_output =
				wlr_scene_get_scene_output(server->scene, o);
			assert(output->scene_output);
		}

		if (output_enabled) {
			wlr_output_layout_move(server->output_layout, o,
				head->state.x, head->state.y);
		}

		if (need_to_remove) {
			wlr_output_layout_remove(server->output_layout, o);
			output->scene_output = NULL;
		}
	}

	server->pending_output_layout_change--;
	do_output_layout_change(server);
}

static bool
verify_output_config_v1(const struct wlr_output_configuration_v1 *config)
{
	/* TODO implement */
	return true;
}

static void
handle_output_manager_apply(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, output_manager_apply);
	struct wlr_output_configuration_v1 *config = data;

	bool config_is_good = verify_output_config_v1(config);

	if (config_is_good) {
		output_config_apply(server, config);
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}
	wlr_output_configuration_v1_destroy(config);
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_xcursor_manager_load(server->seat.xcursor_manager,
			output->wlr_output->scale);
	}
}

/*
 * Take the way outputs are currently configured/layed out and turn that into
 * a struct that we send to clients via the wlr_output_configuration v1
 * interface
 */
static struct
wlr_output_configuration_v1 *create_output_config(struct server *server)
{
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	if (!config) {
		wlr_log(WLR_ERROR, "wlr_output_configuration_v1_create()");
		return NULL;
	}

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct wlr_output_configuration_head_v1 *head =
			wlr_output_configuration_head_v1_create(config,
				output->wlr_output);
		if (!head) {
			wlr_log(WLR_ERROR,
				"wlr_output_configuration_head_v1_create()");
			wlr_output_configuration_v1_destroy(config);
			return NULL;
		}
		struct wlr_box box;
		wlr_output_layout_get_box(server->output_layout,
			output->wlr_output, &box);
		if (!wlr_box_empty(&box)) {
			head->state.x = box.x;
			head->state.y = box.y;
		} else {
			wlr_log(WLR_ERROR, "failed to get output layout box");
		}
	}
	return config;
}

static void
do_output_layout_change(struct server *server)
{
	if (!server->pending_output_layout_change) {
		struct wlr_output_configuration_v1 *config =
			create_output_config(server);
		if (config) {
			wlr_output_manager_v1_set_configuration(
				server->output_manager, config);
		} else {
			wlr_log(WLR_ERROR,
				"wlr_output_manager_v1_set_configuration()");
		}
		output_update_for_layout_change(server);
	}
}

static void
handle_output_layout_change(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, output_layout_change);
	do_output_layout_change(server);
}

void
output_manager_init(struct server *server)
{
	server->output_manager = wlr_output_manager_v1_create(server->wl_display);

	server->output_layout_change.notify = handle_output_layout_change;
	wl_signal_add(&server->output_layout->events.change,
		&server->output_layout_change);

	server->output_manager_apply.notify = handle_output_manager_apply;
	wl_signal_add(&server->output_manager->events.apply,
		&server->output_manager_apply);
}

struct output *
output_from_wlr_output(struct server *server, struct wlr_output *wlr_output)
{
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			return output;
		}
	}
	return NULL;
}

/* returns true if usable area changed */
static bool
update_usable_area(struct output *output)
{
	struct wlr_box old = output->usable_area;
	layers_arrange(output);

	return !wlr_box_equal(&old, &output->usable_area);
}

void
output_update_usable_area(struct output *output)
{
	if (update_usable_area(output)) {
		desktop_arrange_all_views(output->server);
	}
}

void
output_update_all_usable_areas(struct server *server, bool enforce_view_arrange)
{
	bool usable_area_changed = false;
	struct output *output;

	wl_list_for_each(output, &server->outputs, link) {
		usable_area_changed |= update_usable_area(output);
	}
	if (usable_area_changed || enforce_view_arrange) {
		desktop_arrange_all_views(server);
	}
}

struct wlr_box
output_usable_area_in_layout_coords(struct output *output)
{
	if (!output) {
		return (struct wlr_box){0};
	}
	struct wlr_box box = output->usable_area;
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	box.x -= ox;
	box.y -= oy;
	return box;
}

struct wlr_box
output_usable_area_from_cursor_coords(struct server *server)
{
	struct wlr_output *wlr_output;
	wlr_output = wlr_output_layout_output_at(server->output_layout,
		server->seat.cursor->x, server->seat.cursor->y);
	struct output *output = output_from_wlr_output(server, wlr_output);
	return output_usable_area_in_layout_coords(output);
}

void
handle_output_power_manager_set_mode(struct wl_listener *listener, void *data)
{
	struct wlr_output_power_v1_set_mode_event *event = data;

	switch (event->mode) {
	case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
		wlr_output_enable(event->output, false);
		wlr_output_commit(event->output);
		break;
	case ZWLR_OUTPUT_POWER_V1_MODE_ON:
		wlr_output_enable(event->output, true);
		if (!wlr_output_test(event->output)) {
			wlr_output_rollback(event->output);
		}
		wlr_output_commit(event->output);
		break;
	}
}
