// SPDX-License-Identifier: GPL-2.0-only
/*
 * output.c: labwc output and rendering
 *
 * Copyright (C) 2019-2021 Johan Malm
 * Copyright (C) 2020 The Sway authors
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <strings.h>
#include <wlr/backend/drm.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/region.h>
#include <wlr/util/log.h>
#include "common/direction.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "labwc.h"
#include "layers.h"
#include "node.h"
#include "output-state.h"
#include "output-virtual.h"
#include "protocols/cosmic-workspaces.h"
#include "regions.h"
#include "view.h"
#include "xwayland.h"

bool
output_get_tearing_allowance(struct output *output)
{
	struct server *server = output->server;

	/* never allow tearing when disabled */
	if (!rc.allow_tearing) {
		return false;
	}

	struct view *view = server->active_view;

	/*
	 * FIXME: The folling !view check prevents X11 unmanaged surfaces from tearing.
	 *        Unmanaged surfaces also can't be toggled because they have no view.
	 *
	 *        We could potentially do something like:
	 *        - add a force_tearing member to struct xwayland_unmanaged
	 *        - find the struct xwayland_unmanaged based on
	 *          wlr_seat->keyboard_state.focused_surface and server->unmanaged_surfaces
	 *          This could be implemented in some helper like
	 *          xwayland_unmanaged_try_from_wlr_surface(struct server, struct wlr_surface)
	 *        - for LAB_TEARING_ENABLED and LAB_TEARING_ALWAYS this should be enough
	 *        - for LAB_TEARING_FULLSCREEN and LAB_TEARING_FULLSCREEN_FORCED
	 *          compare its size and position with the fullscreen geometry of `output`.
	 *          This could be implemented in some helper like
	 *          bool xwayland_unamanged_is_fullscreen(struct xwayland_unmanaged, struct output)
	 */

	/* this includes X11 unmanaged surfaces but they still can't be toggled */
	if (rc.allow_tearing == LAB_TEARING_ALWAYS) {
		if (view && view->force_tearing == LAB_STATE_DISABLED
				&& view->output == output) {
			return false;
		}
		return true;
	}

	/* tearing is only allowed for the output with the active view */
	if (!view || view->output != output) {
		return false;
	}

	/* allow tearing for any window when requested or forced */
	if (rc.allow_tearing == LAB_TEARING_ENABLED) {
		if (view->force_tearing == LAB_STATE_UNSPECIFIED) {
			return view->tearing_hint;
		} else {
			return view->force_tearing == LAB_STATE_ENABLED;
		}
	}

	/* remaining tearing options apply only to full-screen windows */
	if (!view->fullscreen) {
		return false;
	}

	if (view->force_tearing == LAB_STATE_UNSPECIFIED) {
		/* honor the tearing hint or the fullscreen-force preference */
		return view->tearing_hint ||
			rc.allow_tearing == LAB_TEARING_FULLSCREEN_FORCED;
	}

	/* honor tearing as requested by action */
	return view->force_tearing == LAB_STATE_ENABLED;
}

static void
output_apply_gamma(struct output *output)
{
	assert(output);
	assert(output->gamma_lut_changed);

	struct server *server = output->server;
	struct wlr_scene_output *scene_output = output->scene_output;

	struct wlr_output_state pending;
	wlr_output_state_init(&pending);

	output->gamma_lut_changed = false;
	struct wlr_gamma_control_v1 *gamma_control =
		wlr_gamma_control_manager_v1_get_control(
			server->gamma_control_manager_v1,
			output->wlr_output);

	if (!wlr_gamma_control_v1_apply(gamma_control, &pending)) {
		wlr_output_state_finish(&pending);
		return;
	}

	if (!lab_wlr_scene_output_commit(scene_output, &pending)) {
		wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
	}

	wlr_output_state_finish(&pending);
}

static void
output_frame_notify(struct wl_listener *listener, void *data)
{
	/*
	 * This function is called every time an output is ready to display a
	 * frame - which is typically at 60 Hz.
	 */
	struct output *output = wl_container_of(listener, output, frame);
	if (!output_is_usable(output)) {
		return;
	}

	/*
	 * skip painting the session when it exists but is not active.
	 */
	if (output->server->session && !output->server->session->active) {
		return;
	}

	if (!output->scene_output) {
		/*
		 * TODO: This is a short term fix for issue #1667,
		 *       a proper fix would require restructuring
		 *       the life cycle of scene outputs, e.g.
		 *       creating them on new_output_notify() only.
		 */
		wlr_log(WLR_INFO, "Failed to render new frame: no scene-output");
		return;
	}

	if (output->gamma_lut_changed) {
		/*
		 * We are not mixing the gamma state with
		 * other pending output changes to make it
		 * easier to handle a failed output commit
		 * due to gamma without impacting other
		 * unrelated output changes.
		 */
		output_apply_gamma(output);
	} else {
		struct wlr_scene_output *scene_output = output->scene_output;
		struct wlr_output_state *pending = &output->pending;

		pending->tearing_page_flip = output_get_tearing_allowance(output);

		lab_wlr_scene_output_commit(scene_output, pending);
	}

	struct timespec now = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(output->scene_output, &now);
}

static void
output_destroy_notify(struct wl_listener *listener, void *data)
{
	struct output *output = wl_container_of(listener, output, destroy);
	struct seat *seat = &output->server->seat;
	regions_evacuate_output(output);
	regions_destroy(seat, &output->regions);
	if (seat->overlay.active.output == output) {
		overlay_hide(seat);
	}
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->request_state.link);
	seat_output_layout_changed(seat);

	for (size_t i = 0; i < ARRAY_SIZE(output->layer_tree); i++) {
		wlr_scene_node_destroy(&output->layer_tree[i]->node);
	}
	wlr_scene_node_destroy(&output->layer_popup_tree->node);
	wlr_scene_node_destroy(&output->osd_tree->node);
	wlr_scene_node_destroy(&output->session_lock_tree->node);
	if (output->workspace_osd) {
		wlr_scene_node_destroy(&output->workspace_osd->node);
		output->workspace_osd = NULL;
	}

	struct view *view;
	struct server *server = output->server;
	wl_list_for_each(view, &server->views, link) {
		if (view->output == output) {
			view_on_output_destroy(view);
		}
	}

	wlr_output_state_finish(&output->pending);

	/*
	 * Ensure that we don't accidentally try to dereference
	 * the output pointer in some output event handler like
	 * set_gamma.
	 */
	output->wlr_output->data = NULL;

	/*
	 * output->scene_output (if still around at this point) is
	 * destroyed automatically when the wlr_output is destroyed
	 */
	free(output);
}

static void
output_request_state_notify(struct wl_listener *listener, void *data)
{
	/* This ensures nested backends can be resized */
	struct output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;

	/*
	 * If wlroots ever requests other state changes here we could
	 * restore more of ddc9047a67cd53b2948f71fde1bbe9118000dd3f.
	 */
	if (event->state->committed == WLR_OUTPUT_STATE_MODE) {
		/* Only the mode has changed */
		switch (event->state->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			wlr_output_state_set_mode(&output->pending,
				event->state->mode);
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			wlr_output_state_set_custom_mode(&output->pending,
				event->state->custom_mode.width,
				event->state->custom_mode.height,
				event->state->custom_mode.refresh);
			break;
		}
		wlr_output_schedule_frame(output->wlr_output);
		return;
	}

	/*
	 * Fallback path for everything that we didn't handle above.
	 * The commit will cause a black frame injection so this
	 * path causes flickering during resize of nested outputs.
	 */
	if (!wlr_output_commit_state(output->wlr_output, event->state)) {
		wlr_log(WLR_ERROR, "Backend requested a new state that could not be applied");
	}
}

static void do_output_layout_change(struct server *server);

static bool
can_reuse_mode(struct output *output)
{
	struct wlr_output *wo = output->wlr_output;
	return wo->current_mode && wlr_output_test_state(wo, &output->pending);
}

static void
add_output_to_layout(struct server *server, struct output *output)
{
	struct wlr_output *wlr_output = output->wlr_output;
	struct wlr_output_layout_output *layout_output =
		wlr_output_layout_add_auto(server->output_layout, wlr_output);
	if (!layout_output) {
		wlr_log(WLR_ERROR, "unable to add output to layout");
		return;
	}

	if (!output->scene_output) {
		output->scene_output =
			wlr_scene_output_create(server->scene, wlr_output);
		if (!output->scene_output) {
			wlr_log(WLR_ERROR, "unable to create scene output");
			return;
		}
		/*
		 * Note: wlr_scene_output_layout_add_output() is not
		 * safe to call twice, so we call it only when initially
		 * creating the scene_output.
		 */
		wlr_scene_output_layout_add_output(server->scene_layout,
			layout_output, output->scene_output);
	}

	lab_cosmic_workspace_group_output_enter(
		server->workspaces.cosmic_group, output->wlr_output);

	/* (Re-)create regions from config */
	regions_reconfigure_output(output);

	/* Create lock surface if needed */
	if (server->session_lock_manager->locked) {
		session_lock_output_create(server->session_lock_manager, output);
	}
}

static void
configure_new_output(struct server *server, struct output *output)
{
	struct wlr_output *wlr_output = output->wlr_output;

	wlr_log(WLR_DEBUG, "enable output");
	wlr_output_state_set_enabled(&output->pending, true);

	/*
	 * Try to re-use the existing mode if configured to do so.
	 * Failing that, try to set the preferred mode.
	 */
	struct wlr_output_mode *preferred_mode = NULL;
	if (!rc.reuse_output_mode || !can_reuse_mode(output)) {
		wlr_log(WLR_DEBUG, "set preferred mode");
		/* The mode is a tuple of (width, height, refresh rate). */
		preferred_mode = wlr_output_preferred_mode(wlr_output);
		if (preferred_mode) {
			wlr_output_state_set_mode(&output->pending,
				preferred_mode);
		}
	}

	/*
	 * Sometimes the preferred mode is not available due to hardware
	 * constraints (e.g. GPU or cable bandwidth limitations). In these
	 * cases it's better to fallback to lower modes than to end up with
	 * a black screen. See sway@4cdc4ac6
	 */
	if (!wlr_output_test_state(wlr_output, &output->pending)) {
		wlr_log(WLR_DEBUG,
			"preferred mode rejected, falling back to another mode");
		struct wlr_output_mode *mode;
		wl_list_for_each(mode, &wlr_output->modes, link) {
			if (mode == preferred_mode) {
				continue;
			}
			wlr_output_state_set_mode(&output->pending, mode);
			if (wlr_output_test_state(wlr_output, &output->pending)) {
				break;
			}
		}
	}

	if (rc.adaptive_sync == LAB_ADAPTIVE_SYNC_ENABLED) {
		output_enable_adaptive_sync(output, true);
	}

	output_state_commit(output);

	wlr_output_effective_resolution(wlr_output,
		&output->usable_area.width, &output->usable_area.height);

	/*
	 * Wait until wlr_output_layout_add_auto() returns before
	 * calling do_output_layout_change(); this ensures that the
	 * wlr_output_cursor is created for the new output.
	 */
	server->pending_output_layout_change++;
	add_output_to_layout(server, output);
	server->pending_output_layout_change--;
}

static void
new_output_notify(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised by the backend when a new output (aka display
	 * or monitor) becomes available.
	 */
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			/*
			 * This is a duplicated notification.
			 * We may end up here when a virtual output
			 * was added before the headless backend was
			 * started up.
			 */
			return;
		}
	}

	if (wlr_output_is_wl(wlr_output)) {
		char title[64];
		snprintf(title, sizeof(title), "%s - %s", "labwc", wlr_output->name);
		wlr_wl_output_set_title(wlr_output, title);
		wlr_wl_output_set_app_id(wlr_output, "labwc");
	}

	/*
	 * We offer any display as available for lease, some apps like
	 * gamescope want to take ownership of a display when they can
	 * to use planes and present directly.
	 * This is also useful for debugging the DRM parts of
	 * another compositor.
	 */
	if (server->drm_lease_manager && wlr_output_is_drm(wlr_output)) {
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
	 * and our renderer. Must be done once, before committing the output
	 */
	if (!wlr_output_init_render(wlr_output, server->allocator,
			server->renderer)) {
		wlr_log(WLR_ERROR, "unable to init output renderer");
		return;
	}

	output = znew(*output);
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;
	output_state_init(output);

	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->request_state.notify = output_request_state_notify;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	wl_list_init(&output->regions);

	/*
	 * Create layer-trees (background, bottom, top and overlay) and
	 * a layer-popup-tree.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(output->layer_tree); i++) {
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
	output->session_lock_tree = wlr_scene_tree_create(&server->scene->tree);
	node_descriptor_create(&output->session_lock_tree->node,
		LAB_NODE_DESC_TREE, NULL);

	/*
	 * Set the z-positions to achieve the following order (from top to
	 * bottom):
	 *	- session lock layer
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
	wlr_scene_node_raise_to_top(&output->session_lock_tree->node);

	configure_new_output(server, output);
	do_output_layout_change(server);
}

void
output_init(struct server *server)
{
	server->gamma_control_manager_v1 =
		wlr_gamma_control_manager_v1_create(server->wl_display);

	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/*
	 * Create an output layout, which is a wlroots utility for working with
	 * an arrangement of screens in a physical layout.
	 */
	server->output_layout = wlr_output_layout_create(server->wl_display);
	if (!server->output_layout) {
		wlr_log(WLR_ERROR, "unable to create output layout");
		exit(EXIT_FAILURE);
	}
	server->scene_layout = wlr_scene_attach_output_layout(server->scene,
		server->output_layout);
	if (!server->scene_layout) {
		wlr_log(WLR_ERROR, "unable to create scene layout");
		exit(EXIT_FAILURE);
	}

	/* Enable screen recording with wf-recorder */
	wlr_xdg_output_manager_v1_create(server->wl_display,
		server->output_layout);

	wl_list_init(&server->outputs);

	output_manager_init(server);
}

static void
output_update_for_layout_change(struct server *server)
{
	output_update_all_usable_areas(server, /*layout_changed*/ true);
	session_lock_update_for_layout_change(server);

	/*
	 * "Move" each wlr_output_cursor (in per-output coordinates) to
	 * align with the seat cursor. Re-set the cursor image so that
	 * the cursor isn't invisible on new outputs.
	 */
	wlr_cursor_move(server->seat.cursor, NULL, 0, 0);
	cursor_update_image(&server->seat);
}

static bool
output_config_apply(struct server *server,
		struct wlr_output_configuration_v1 *config)
{
	bool success = true;
	server->pending_output_layout_change++;

	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each(head, &config->heads, link) {
		struct wlr_output *o = head->state.output;
		struct output *output = output_from_wlr_output(server, o);
		struct wlr_output_state *os = &output->pending;
		bool output_enabled = head->state.enabled && !output->leased;
		bool need_to_add = output_enabled && !o->enabled;
		bool need_to_remove = !output_enabled && o->enabled;

		wlr_output_state_set_enabled(os, output_enabled);
		if (output_enabled) {
			/* Output specific actions only */
			if (head->state.mode) {
				wlr_output_state_set_mode(os, head->state.mode);
			} else {
				wlr_output_state_set_custom_mode(os,
					head->state.custom_mode.width,
					head->state.custom_mode.height,
					head->state.custom_mode.refresh);
			}
			wlr_output_state_set_scale(os, head->state.scale);
			wlr_output_state_set_transform(os, head->state.transform);
			output_enable_adaptive_sync(output,
				head->state.adaptive_sync_enabled);
		}
		if (!output_state_commit(output)) {
			/*
			 * FIXME: This is only part of the story, we should revert
			 *        all previously commited outputs as well here.
			 *
			 *        See https://github.com/labwc/labwc/pull/1528
			 */
			wlr_log(WLR_INFO, "Output config commit failed: %s", o->name);
			success = false;
			break;
		}

		/* Only do Layout specific actions if the commit went trough */
		if (need_to_add) {
			add_output_to_layout(server, output);
		}

		if (output_enabled) {
			struct wlr_box pos = {0};
			wlr_output_layout_get_box(server->output_layout, o, &pos);
			if (pos.x != head->state.x || pos.y != head->state.y) {
				/*
				 * This overrides the automatic layout
				 *
				 * wlr_output_layout_add() in fact means _move()
				 */
				wlr_output_layout_add(server->output_layout, o,
					head->state.x, head->state.y);
			}
		}

		if (need_to_remove) {
			regions_evacuate_output(output);

			lab_cosmic_workspace_group_output_leave(
				server->workspaces.cosmic_group, output->wlr_output);

			/*
			 * At time of writing, wlr_output_layout_remove()
			 * indirectly destroys the wlr_scene_output, but
			 * this behavior may change in future. To remove
			 * doubt and avoid either a leak or double-free,
			 * explicitly destroy the wlr_scene_output before
			 * calling wlr_output_layout_remove().
			 */
			wlr_scene_output_destroy(output->scene_output);
			wlr_output_layout_remove(server->output_layout, o);
			output->scene_output = NULL;
		}
	}

	server->pending_output_layout_change--;
	do_output_layout_change(server);
	return success;
}

static bool
verify_output_config_v1(const struct wlr_output_configuration_v1 *config)
{
	const char *err_msg = NULL;
	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each(head, &config->heads, link) {
		if (!head->state.enabled) {
			continue;
		}

		/* Handle custom modes */
		if (!head->state.mode) {
			int32_t refresh = head->state.custom_mode.refresh;

			if (wlr_output_is_drm(head->state.output) && refresh == 0) {
				/*
				 * wlroots has a bug which causes a divide by zero
				 * when setting the refresh rate to 0 on a DRM output.
				 * It is already fixed but not part of an official 0.17.x
				 * release. Even it would be we still need to carry the
				 * fix here to prevent older 0.17.x releases from crashing.
				 *
				 * https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3791
				 */
				err_msg = "DRM backend does not support a refresh rate of 0";
				goto custom_mode_failed;
			}

			if (wlr_output_is_wl(head->state.output) && refresh != 0) {
				/* Wayland backend does not support refresh rates */
				err_msg = "Wayland backend refresh rates unsupported";
				goto custom_mode_failed;
			}
		}

		if (wlr_output_is_wl(head->state.output)
				&& !head->state.adaptive_sync_enabled) {
			err_msg = "Wayland backend requires adaptive sync";
			goto custom_mode_failed;
		}

		/*
		 * Ensure the new output state can be applied on
		 * its own and inform the client when it can not.
		 *
		 * Applying the changes may still fail later when
		 * getting mixed with wlr_output->pending which
		 * may contain further unrelated changes.
		 */
		struct wlr_output_state output_state;
		wlr_output_state_init(&output_state);
		wlr_output_head_v1_state_apply(&head->state, &output_state);

		if (!wlr_output_test_state(head->state.output, &output_state)) {
			wlr_output_state_finish(&output_state);
			return false;
		}
		wlr_output_state_finish(&output_state);
	}

	return true;

custom_mode_failed:
	assert(err_msg);
	wlr_log(WLR_INFO, "%s (%s: %dx%d@%d)",
		err_msg,
		head->state.output->name,
		head->state.custom_mode.width,
		head->state.custom_mode.height,
		head->state.custom_mode.refresh);
	return false;
}

static void
handle_output_manager_test(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;

	if (verify_output_config_v1(config)) {
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}
	wlr_output_configuration_v1_destroy(config);
}

static void
handle_output_manager_apply(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, output_manager_apply);
	struct wlr_output_configuration_v1 *config = data;

	bool config_is_good = verify_output_config_v1(config);

	if (config_is_good && output_config_apply(server, config)) {
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

	/* Re-set cursor image in case scale changed */
	cursor_update_focus(server);
	cursor_update_image(&server->seat);
}

/*
 * Take the way outputs are currently configured/laid out and turn that into
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
		seat_output_layout_changed(&server->seat);
	}
}

static void
handle_output_layout_change(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, output_layout_change);

	/* Prevents unnecessary layout recalculations */
	server->pending_output_layout_change++;
	output_virtual_update_fallback(server);
	server->pending_output_layout_change--;

	do_output_layout_change(server);
}

static void
handle_gamma_control_set_gamma(struct wl_listener *listener, void *data)
{
	const struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;

	struct output *output = event->output->data;
	if (!output_is_usable(output)) {
		return;
	}
	output->gamma_lut_changed = true;
	wlr_output_schedule_frame(output->wlr_output);
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

	server->output_manager_test.notify = handle_output_manager_test;
	wl_signal_add(&server->output_manager->events.test,
		&server->output_manager_test);

	server->gamma_control_set_gamma.notify = handle_gamma_control_set_gamma;
	wl_signal_add(&server->gamma_control_manager_v1->events.set_gamma,
		&server->gamma_control_set_gamma);
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

struct output *
output_from_name(struct server *server, const char *name)
{
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (!output_is_usable(output) || !output->wlr_output->name) {
			continue;
		}
		if (!strcasecmp(name, output->wlr_output->name)) {
			return output;
		}
	}
	return NULL;
}

struct output *
output_nearest_to(struct server *server, int lx, int ly)
{
	double closest_x, closest_y;
	wlr_output_layout_closest_point(server->output_layout, NULL, lx, ly,
		&closest_x, &closest_y);

	return output_from_wlr_output(server,
		wlr_output_layout_output_at(server->output_layout,
			closest_x, closest_y));
}

struct output *
output_nearest_to_cursor(struct server *server)
{
	return output_nearest_to(server, server->seat.cursor->x,
		server->seat.cursor->y);
}

struct output *
output_get_adjacent(struct output *output, enum view_edge edge, bool wrap)
{
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR,
			"output is not usable, cannot find adjacent output");
		return NULL;
	}

	struct wlr_box box = output_usable_area_in_layout_coords(output);
	int lx = box.x + box.width / 2;
	int ly = box.y + box.height / 2;

	/* Determine any adjacent output in the appropriate direction */
	struct wlr_output *new_output = NULL;
	struct wlr_output *current_output = output->wlr_output;
	struct wlr_output_layout *layout = output->server->output_layout;
	enum wlr_direction direction = direction_from_view_edge(edge);
	new_output = wlr_output_layout_adjacent_output(layout, direction,
		current_output, lx, ly);

	/*
	 * Optionally wrap around from top-to-bottom or left-to-right, and vice
	 * versa.
	 */
	if (wrap && !new_output) {
		new_output = wlr_output_layout_farthest_output(layout,
			direction_get_opposite(direction), current_output, lx, ly);
	}

	/*
	 * When "adjacent" output is the same as the original, there is no
	 * adjacent
	 */
	if (!new_output || new_output == current_output) {
		return NULL;
	}

	output = output_from_wlr_output(output->server, new_output);
	if (!output_is_usable(output)) {
		wlr_log(WLR_ERROR, "invalid output in layout");
		return NULL;
	}

	return output;
}

bool
output_is_usable(struct output *output)
{
	/* output_is_usable(NULL) is safe and returns false */
	return output && output->wlr_output->enabled && !output->leased;
}

/* returns true if usable area changed */
static bool
update_usable_area(struct output *output)
{
	struct wlr_box old = output->usable_area;
	layers_arrange(output);

#if HAVE_XWAYLAND
	struct view *view;
	wl_list_for_each(view, &output->server->views, link) {
		if (view->mapped && view->type == LAB_XWAYLAND_VIEW) {
			xwayland_adjust_usable_area(view,
				output->server->output_layout,
				output->wlr_output, &output->usable_area);
		}
	}
#endif
	return !wlr_box_equal(&old, &output->usable_area);
}

void
output_update_usable_area(struct output *output)
{
	if (update_usable_area(output)) {
		regions_update_geometry(output);
#if HAVE_XWAYLAND
		xwayland_update_workarea(output->server);
#endif
		desktop_arrange_all_views(output->server);
	}
}

void
output_update_all_usable_areas(struct server *server, bool layout_changed)
{
	bool usable_area_changed = false;
	struct output *output;

	wl_list_for_each(output, &server->outputs, link) {
		if (update_usable_area(output)) {
			usable_area_changed = true;
			regions_update_geometry(output);
		} else if (layout_changed) {
			regions_update_geometry(output);
		}
	}
	if (usable_area_changed || layout_changed) {
#if HAVE_XWAYLAND
		xwayland_update_workarea(server);
#endif
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
output_usable_area_scaled(struct output *output)
{
	if (!output) {
		return (struct wlr_box){0};
	}
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	if (usable.height == output->wlr_output->height
			&& output->wlr_output->scale != 1) {
		usable.height /= output->wlr_output->scale;
	}
	if (usable.width == output->wlr_output->width
			&& output->wlr_output->scale != 1) {
		usable.width /= output->wlr_output->scale;
	}
	return usable;
}

void
handle_output_power_manager_set_mode(struct wl_listener *listener, void *data)
{
	struct server *server = wl_container_of(listener, server,
		output_power_manager_set_mode);
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct output *output = event->output->data;
	assert(output);

	switch (event->mode) {
	case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
		wlr_output_state_set_enabled(&output->pending, false);
		output_state_commit(output);
		break;
	case ZWLR_OUTPUT_POWER_V1_MODE_ON:
		wlr_output_state_set_enabled(&output->pending, true);
		output_state_commit(output);
		/*
		 * Re-set the cursor image so that the cursor
		 * isn't invisible on the newly enabled output.
		 */
		cursor_update_image(&server->seat);
		break;
	}
}

void
output_enable_adaptive_sync(struct output *output, bool enabled)
{
	assert(output_is_usable(output));

	wlr_output_state_set_adaptive_sync_enabled(&output->pending, enabled);
	if (!wlr_output_test_state(output->wlr_output, &output->pending)) {
		wlr_output_state_set_adaptive_sync_enabled(&output->pending, false);
		wlr_log(WLR_DEBUG,
			"failed to enable adaptive sync for output %s",
			output->wlr_output->name);
	} else {
		wlr_log(WLR_INFO, "adaptive sync %sabled for output %s",
			enabled ? "en" : "dis", output->wlr_output->name);
	}
}

float
output_max_scale(struct server *server)
{
	/* Never return less than 1, in case outputs are disabled */
	float scale = 1;
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output_is_usable(output)) {
			scale = MAX(scale, output->wlr_output->scale);
		}
	}
	return scale;
}
